/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gmodule.h>
#include <glib.h>
#include <string>
#include <packagekit-glib/packagekit.h>

#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/pkgrecords.h>

#include "apt.h"
#include "apt-utils.h"
#include "matcher.h"

#include <config.h>

#include <locale.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include <iomanip>

using namespace std;

#include <pk-backend.h>

/* static bodges */
static bool _cancel = false;
static guint _progress_percentage = 0;
static gulong _signal_timeout = 0;
static gchar **_package_ids;
static const gchar *_search;
static guint _package_current = 0;
static gboolean _repo_enabled_local = false;
static gboolean _repo_enabled_fedora = true;
static gboolean _repo_enabled_devel = true;
static gboolean _repo_enabled_livna = true;
static gboolean _updated_gtkhtml = false;
static gboolean _updated_kernel = false;
static gboolean _updated_powertop = false;
static gboolean _has_signature = false;

static pkgSourceList *apt_source_list = 0;

/**
 * backend_initialize:
 */
static void
backend_initialize (PkBackend *backend)
{
	_progress_percentage = 0;
	egg_debug ("APTcc Initializing");

	if (pkgInitConfig(*_config) == false ||
	    pkgInitSystem(*_config,_system) == false)
	{
		egg_debug ("ERROR initializing backend");
	}

	// Open the cache file
	apt_source_list = new pkgSourceList;
	apt_source_list->ReadMainList();
}

/**
 * backend_destroy:
 */
static void
backend_destroy (PkBackend *backend)
{
	egg_debug ("APTcc being destroyed");
	if (apt_source_list)
	{
		delete apt_source_list;
		apt_source_list = NULL;
	}
}

/**
 * backend_get_groups:
 */
static PkBitfield
backend_get_groups (PkBackend *backend)
{
	return pk_bitfield_from_enums (
		PK_GROUP_ENUM_ACCESSORIES,
		PK_GROUP_ENUM_ADMIN_TOOLS,
		PK_GROUP_ENUM_COMMUNICATION,
		PK_GROUP_ENUM_DOCUMENTATION,
		PK_GROUP_ENUM_DESKTOP_GNOME,
		PK_GROUP_ENUM_DESKTOP_KDE,
		PK_GROUP_ENUM_DESKTOP_OTHER,
		PK_GROUP_ENUM_ELECTRONICS,
		PK_GROUP_ENUM_GAMES,
		PK_GROUP_ENUM_GRAPHICS,
		PK_GROUP_ENUM_INTERNET,
		PK_GROUP_ENUM_LEGACY,
		PK_GROUP_ENUM_LOCALIZATION,
		PK_GROUP_ENUM_MULTIMEDIA,
		PK_GROUP_ENUM_NETWORK,
		PK_GROUP_ENUM_OTHER,
		PK_GROUP_ENUM_PROGRAMMING,
		PK_GROUP_ENUM_PUBLISHING,
		PK_GROUP_ENUM_SCIENCE,
		PK_GROUP_ENUM_SYSTEM,
		PK_GROUP_ENUM_COLLECTIONS,
		-1);
}

/**
 * backend_get_filters:
 */
static PkBitfield
backend_get_filters (PkBackend *backend)
{
	return pk_bitfield_from_enums (
		PK_FILTER_ENUM_GUI,
		PK_FILTER_ENUM_INSTALLED,
		PK_FILTER_ENUM_DEVELOPMENT,
		PK_FILTER_ENUM_FREE,
		PK_FILTER_ENUM_COLLECTIONS,
		-1);
}

/**
 * backend_get_mime_types:
 */
static gchar *
backend_get_mime_types (PkBackend *backend)
{
	return g_strdup ("application/x-deb");
}

/**
 * backend_cancel_timeout:
 */
static gboolean
backend_cancel_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;

	/* we can now cancel again */
	_signal_timeout = 0;

	/* now mark as finished */
	pk_backend_error_code (backend, PK_ERROR_ENUM_TRANSACTION_CANCELLED,
			       "The task was stopped successfully");
	pk_backend_finished (backend);
	return false;
}

/**
 * backend_cancel:
 */
static void
backend_cancel (PkBackend *backend)
{
	/* cancel the timeout */
	if (_signal_timeout != 0) {
		g_source_remove (_signal_timeout);

		/* emulate that it takes us a few ms to cancel */
		g_timeout_add (1500, backend_cancel_timeout, backend);
	}
	_cancel = true;
}

static gboolean
backend_get_depends_or_requires_thread (PkBackend *backend)
{
	gchar **package_ids;
	PkBitfield filters;

	package_ids = pk_backend_get_strv (backend, "package_ids");
	filters = (PkBitfield) pk_backend_get_uint (backend, "filters");
	_cancel = false;
	pk_backend_set_allow_cancel (backend, true);
	PkPackageId *pi = pk_package_id_new_from_string (package_ids[0]);
	if (pi == NULL) {
		pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_ID_INVALID, "invalid package id");
		pk_backend_finished (backend);
		return false;
	}

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);

	aptcc *m_apt = new aptcc();
	if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
		egg_debug ("Failed to create apt cache");
		delete m_apt;
		return false;
	}

	bool depends = pk_backend_get_bool(backend, "get_depends");

	vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > output;
	for (uint i = 0; i < g_strv_length(package_ids); i++) {
		if (_cancel) {
			break;
		}
		pi = pk_package_id_new_from_string (package_ids[i]);
		if (pi == NULL) {
			pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_ID_INVALID, "invalid package id");
			pk_backend_finished (backend);
			delete m_apt;
			return false;
		}

		pkgCache::PkgIterator pkg = m_apt->cacheFile->FindPkg(pi->name);
		if (pkg.end() == true)
		{
			pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_NOT_FOUND, "couldn't find package");
			pk_package_id_free (pi);
			pk_backend_finished (backend);
			delete m_apt;
			return false;
		}

		vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > pkgOutput;
		if (depends) {
			pkgOutput = m_apt->get_depends(pkg, false, _cancel);
		} else {
			pkgOutput = m_apt->get_requires(pkg, false, _cancel);
		}
		vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator it;
		it = pkgOutput.begin();
		output.insert(output.end(), it, pkgOutput.end());

		pk_package_id_free (pi);
	}

	sort(output.begin(), output.end(), compare());
	output.erase(unique(output.begin(), output.end(), result_equality()),
		     output.end());

	// It's faster to emmit the packages here than in the matching part
	for(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator i=output.begin();
	    i != output.end(); ++i)
	{
		if (_cancel) {
			break;
		}
		m_apt->emit_package(backend, filters, i->first, i->second);
	}

	delete m_apt;

	pk_backend_finished (backend);
	return true;
}

/**
 * backend_get_depends:
 */
static void
backend_get_depends (PkBackend *backend, PkBitfield filters, gchar **package_ids, gboolean recursive)
{
	pk_backend_set_bool (backend, "get_depends", true);
	pk_backend_thread_create (backend, backend_get_depends_or_requires_thread);
}

/**
 * backend_get_requires:
 */
static void
backend_get_requires (PkBackend *backend, PkBitfield filters, gchar **package_ids, gboolean recursive)
{
	pk_backend_set_bool (backend, "get_depends", false);
	pk_backend_thread_create (backend, backend_get_depends_or_requires_thread);
}

static gboolean
backend_get_details_thread (PkBackend *backend)
{
	gchar **package_ids;
	PkPackageId *pi;

	package_ids = pk_backend_get_strv (backend, "package_ids");
	if (package_ids == NULL) {
		pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_ID_INVALID, "invalid package id");
		pk_backend_finished (backend);
		return false;
	}

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);

	aptcc *m_apt = new aptcc();
	if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
		egg_debug ("Failed to create apt cache");
		delete m_apt;
		return false;
	}
	
	for (uint i = 0; i < g_strv_length(package_ids); i++) {
		pi = pk_package_id_new_from_string (package_ids[i]);
		if (pi == NULL) {
			pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_ID_INVALID, "invalid package id");
			pk_backend_finished (backend);
			delete m_apt;
			return false;
		}

		pkgCache::PkgIterator pkg = m_apt->cacheFile->FindPkg(pi->name);
		if (pkg.end() == true)
		{
			pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_NOT_FOUND, "couldn't find package");
			pk_package_id_free (pi);
			pk_backend_finished (backend);
			delete m_apt;
			return false;
		}

		m_apt->emit_details(backend, pkg);

		pk_package_id_free (pi);
	}

	delete m_apt;
	
	pk_backend_finished (backend);
	return true;
}

/**
 * backend_get_details:
 */
static void
backend_get_details (PkBackend *backend, gchar **package_ids)
{
	pk_backend_thread_create (backend, backend_get_details_thread);
}

static gboolean
backend_get_files_thread (PkBackend *backend)
{
	gchar **package_ids;
	PkPackageId *pi;

	package_ids = pk_backend_get_strv (backend, "package_ids");
	if (package_ids == NULL) {
		pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_ID_INVALID, "invalid package id");
		pk_backend_finished (backend);
		return false;
	}

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);

	aptcc *m_apt = new aptcc();
	if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
		egg_debug ("Failed to create apt cache");
		delete m_apt;
		return false;
	}

	for (uint i = 0; i < g_strv_length(package_ids); i++) {
		pi = pk_package_id_new_from_string (package_ids[i]);
		if (pi == NULL) {
			pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_ID_INVALID, "invalid package id");
			pk_backend_finished (backend);
			delete m_apt;
			return false;
		}

		pkgCache::PkgIterator Pkg = m_apt->cacheFile->FindPkg(pi->name);
		if (Pkg.end() == true)
		{
			pk_backend_error_code (backend, PK_ERROR_ENUM_PACKAGE_NOT_FOUND, "couldn't find package");
			pk_package_id_free (pi);
			pk_backend_finished (backend);
			delete m_apt;
			return false;
		}

		emit_files (backend, pi);

		pk_package_id_free (pi);
	}

	delete m_apt;

	pk_backend_finished (backend);
	return true;
}

/**
 * backend_get_files:
 */
static void
backend_get_files (PkBackend *backend, gchar **package_ids)
{
	pk_backend_thread_create (backend, backend_get_files_thread);
}

/**
 * backend_get_update_detail_timeout:
 **/
static gboolean
backend_get_update_detail_timeout (gpointer data)
{
	guint i;
	guint len;
	const gchar *package_id;
	PkBackend *backend = (PkBackend *) data;

	/* each one has a different detail for testing */
	len = g_strv_length (_package_ids);
	for (i=0; i<len; i++) {
		package_id = _package_ids[i];
		if (egg_strequal (package_id, "powertop;1.8-1.fc8;i386;fedora")) {
			pk_backend_update_detail (backend, package_id,
						  "powertop;1.7-1.fc8;i386;installed", "",
						  "http://www.distro-update.org/page?moo;Bugfix release for powertop",
						  "http://bgzilla.fd.org/result.php?#12344;Freedesktop Bugzilla #12344",
						  "", PK_RESTART_ENUM_NONE, "Update to newest upstream source",
						  "", PK_UPDATE_STATE_ENUM_STABLE, "2008-07-31", NULL);
		}
		if (egg_strequal (package_id, "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed")) {
			pk_backend_update_detail (backend, package_id,
						  "kernel;2.6.22-0.104.rc3.git6.fc8;i386;installed^"
						  "kernel;2.6.22-0.105.rc3.git7.fc8;i386;installed", "",
						  "http://www.distro-update.org/page?moo;Bugfix release for kernel",
						  "http://bgzilla.fd.org/result.php?#12344;Freedesktop Bugzilla #12344;"
						  "http://bgzilla.gnome.org/result.php?#9876;GNOME Bugzilla #9876",
						  "http://nvd.nist.gov/nvd.cfm?cvename=CVE-2007-3381;CVE-2007-3381",
						  PK_RESTART_ENUM_SYSTEM, "Update to newest version",
						  "", PK_UPDATE_STATE_ENUM_UNSTABLE, "2008-06-28", NULL);
		}
		if (egg_strequal (package_id, "gtkhtml2;2.19.1-4.fc8;i386;fedora")) {
			pk_backend_update_detail (backend, package_id,
						  "gtkhtml2;2.18.1-22.fc8;i386;installed", "",
						  "http://www.distro-update.org/page?moo;Bugfix release for gtkhtml",
						  "http://bgzilla.gnome.org/result.php?#9876;GNOME Bugzilla #9876",
						  NULL, PK_RESTART_ENUM_SESSION,
						  "Update to latest whizz bang version\n"
						  "* support this new thing\n"
						  "* something else\n"
						  "- and that new thing",
						  "", PK_UPDATE_STATE_ENUM_UNKNOWN, "2008-07-25", NULL);
		}
	}
	pk_backend_finished (backend);
	_signal_timeout = 0;
	return false;
}

/**
 * backend_get_update_detail:
 */
static void
backend_get_update_detail (PkBackend *backend, gchar **package_ids)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);
	_package_ids = package_ids;
	_signal_timeout = g_timeout_add (500, backend_get_update_detail_timeout, backend);
}

/**
 * backend_get_updates_timeout:
 **/
static gboolean
backend_get_updates_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;

	if (!_updated_powertop && !_updated_kernel && !_updated_gtkhtml) {
		pk_backend_package (backend, PK_INFO_ENUM_BLOCKED,
				    "vino;2.24.2.fc9;i386;fedora",
				    "Remote desktop server for the desktop");
	}
	if (!_updated_powertop) {
		pk_backend_package (backend, PK_INFO_ENUM_NORMAL,
				    "powertop;1.8-1.fc8;i386;fedora",
				    "Power consumption monitor");
	}
	if (!_updated_kernel) {
		pk_backend_package (backend, PK_INFO_ENUM_SECURITY,
				    "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed",
				    "The Linux kernel (the core of the Linux operating system)");
	}
	if (!_updated_gtkhtml) {
		pk_backend_package (backend, PK_INFO_ENUM_SECURITY,
				    "gtkhtml2;2.19.1-4.fc8;i386;fedora",
				    "An HTML widget for GTK+ 2.0");
	}
	pk_backend_finished (backend);
	_signal_timeout = 0;
	return false;
}

/**
 * backend_get_updates:
 */
static void
backend_get_updates (PkBackend *backend, PkBitfield filters)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);
	pk_backend_set_percentage (backend, PK_BACKEND_PERCENTAGE_INVALID);
	/* check network state */
	if (!pk_backend_is_online (backend)) {
		pk_backend_error_code (backend, PK_ERROR_ENUM_NO_NETWORK, "Cannot check when offline");
		pk_backend_finished (backend);
		return;
	}
	_signal_timeout = g_timeout_add (1000, backend_get_updates_timeout, backend);
}

static gboolean
backend_install_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;
	guint sub_percent;

	if (_progress_percentage == 100) {
		pk_backend_finished (backend);
		return false;
	}
	if (_progress_percentage == 30) {
		pk_backend_set_allow_cancel (backend, false);
		pk_backend_package (backend, PK_INFO_ENUM_INSTALLING,
				    "gtkhtml2;2.19.1-4.fc8;i386;fedora",
				    "An HTML widget for GTK+ 2.0");
		pk_backend_set_status (backend, PK_STATUS_ENUM_INSTALL);
	}
	if (_progress_percentage == 50) {
		pk_backend_package (backend, PK_INFO_ENUM_INSTALLING,
				    "gtkhtml2-devel;2.19.1-0.fc8;i386;fedora",
				    "Devel files for gtkhtml");
		/* this duplicate package should be ignored */
		pk_backend_package (backend, PK_INFO_ENUM_INSTALLING,
				    "gtkhtml2-devel;2.19.1-0.fc8;i386;fedora", NULL);
		pk_backend_set_status (backend, PK_STATUS_ENUM_INSTALL);
	}
	if (_progress_percentage > 30 && _progress_percentage < 50) {
		sub_percent = ((gfloat) (_progress_percentage - 30.0f) / 20.0f) * 100.0f;
		pk_backend_set_sub_percentage (backend, sub_percent);
	} else {
		pk_backend_set_sub_percentage (backend, PK_BACKEND_PERCENTAGE_INVALID);
	}
	_progress_percentage += 1;
	pk_backend_set_percentage (backend, _progress_percentage);
	return true;
}

/**
 * backend_install_packages:
 */
static void
backend_install_packages (PkBackend *backend, gchar **package_ids)
{
	const gchar *license_agreement;
	const gchar *eula_id;
	gboolean has_eula;

	if (egg_strequal (package_ids[0], "vips-doc;7.12.4-2.fc8;noarch;linva")) {
		if (!_has_signature) {
			pk_backend_repo_signature_required (backend, package_ids[0], "updates",
							    "http://example.com/gpgkey",
							    "Test Key (Fedora) fedora@example.com",
							    "BB7576AC",
							    "D8CC 06C2 77EC 9C53 372F C199 B1EE 1799 F24F 1B08",
							    "2007-10-04", PK_SIGTYPE_ENUM_GPG);
			pk_backend_error_code (backend, PK_ERROR_ENUM_GPG_FAILURE,
					       "GPG signed package could not be verified");
			pk_backend_finished (backend);
			return;
		}
		eula_id = "eula_hughsie_dot_com";
		has_eula = pk_backend_is_eula_valid (backend, eula_id);
		if (!has_eula) {
			license_agreement = "Narrator: In A.D. 2101, war was beginning.\n"
					    "Captain: What happen ?\n"
					    "Mechanic: Somebody set up us the bomb.\n\n"
					    "Operator: We get signal.\n"
					    "Captain: What !\n"
					    "Operator: Main screen turn on.\n"
					    "Captain: It's you !!\n"
					    "CATS: How are you gentlemen !!\n"
					    "CATS: All your base are belong to us.\n"
					    "CATS: You are on the way to destruction.\n\n"
					    "Captain: What you say !!\n"
					    "CATS: You have no chance to survive make your time.\n"
					    "CATS: Ha Ha Ha Ha ....\n\n"
					    "Operator: Captain!! *\n"
					    "Captain: Take off every 'ZIG' !!\n"
					    "Captain: You know what you doing.\n"
					    "Captain: Move 'ZIG'.\n"
					    "Captain: For great justice.\n";
			pk_backend_eula_required (backend, eula_id, package_ids[0],
						  "CATS Inc.", license_agreement);
			pk_backend_error_code (backend, PK_ERROR_ENUM_NO_LICENSE_AGREEMENT,
					       "licence not installed so cannot install");
			pk_backend_finished (backend);
			return;
		}
	}

	pk_backend_set_allow_cancel (backend, true);
	_progress_percentage = 0;
	pk_backend_package (backend, PK_INFO_ENUM_DOWNLOADING,
			    "gtkhtml2;2.19.1-4.fc8;i386;fedora",
			    "An HTML widget for GTK+ 2.0");
	_signal_timeout = g_timeout_add (100, backend_install_timeout, backend);
}

/**
 * backend_install_signature:
 */
static void
backend_install_signature (PkBackend *backend, PkSigTypeEnum type,
			   const gchar *key_id, const gchar *package_id)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_INSTALL);
	if (type == PK_SIGTYPE_ENUM_GPG &&
	    egg_strequal (package_id, "vips-doc;7.12.4-2.fc8;noarch;linva") &&
	    egg_strequal (key_id, "BB7576AC")) {
		egg_debug ("installed signature %s for %s", key_id, package_id);
		_has_signature = true;
	} else {
		pk_backend_error_code (backend, PK_ERROR_ENUM_GPG_FAILURE,
				       "GPG key %s not recognised for package_id %s",
				       key_id, package_id);
	}
	pk_backend_finished (backend);
}

/**
 * backend_refresh_cache_timeout:
 */
static gboolean
backend_install_files_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;
	pk_backend_finished (backend);
	return false;
}

/**
 * backend_install_files:
 */
static void
backend_install_files (PkBackend *backend, gboolean trusted, gchar **full_paths)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_INSTALL);
	pk_backend_set_percentage (backend, 101);
	_signal_timeout = g_timeout_add (2000, backend_install_files_timeout, backend);
}

/**
 * backend_refresh_cache_timeout:
 */
static gboolean
backend_refresh_cache_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;
	if (_progress_percentage == 100) {
		pk_backend_finished (backend);
		return false;
	}
	if (_progress_percentage == 80)
		pk_backend_set_allow_cancel (backend, false);
	_progress_percentage += 10;
	pk_backend_set_percentage (backend, _progress_percentage);
	return true;
}

/**
 * backend_refresh_cache:
 */
static void
backend_refresh_cache (PkBackend *backend, gboolean force)
{
	_progress_percentage = 0;

	/* reset */
	_updated_gtkhtml = false;
	_updated_kernel = false;
	_updated_powertop = false;

	pk_backend_set_allow_cancel (backend, true);
	pk_backend_set_status (backend, PK_STATUS_ENUM_REFRESH_CACHE);
	_signal_timeout = g_timeout_add (500, backend_refresh_cache_timeout, backend);
}

/**
 * backend_resolve:
 */
static void
backend_resolve (PkBackend *backend, PkBitfield filters, gchar **packages)
{
	guint i;
	guint len;

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);

	/* each one has a different detail for testing */
	len = g_strv_length (packages);
	for (i=0; i<len; i++) {
		if (egg_strequal (packages[i], "vips-doc"))
			pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE,
					    "vips-doc;7.12.4-2.fc8;noarch;linva", "The vips documentation package.");
		else if (egg_strequal (packages[i], "glib2"))
			pk_backend_package (backend, PK_INFO_ENUM_INSTALLED,
					    "glib2;2.14.0;i386;fedora", "The GLib library");
		else if (egg_strequal (packages[i], "powertop"))
			pk_backend_package (backend, PK_INFO_ENUM_UPDATING,
					    "powertop;1.8-1.fc8;i386;fedora", "Power consumption monitor");
		else if (egg_strequal (packages[i], "kernel"))
			pk_backend_package (backend, PK_INFO_ENUM_UPDATING,
					    "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed", "The Linux kernel (the core of the Linux operating system)");
		else if (egg_strequal (packages[i], "gtkhtml2"))
			pk_backend_package (backend, PK_INFO_ENUM_UPDATING,
					    "gtkhtml2;2.19.1-4.fc8;i386;fedora", "An HTML widget for GTK+ 2.0");
	}
	pk_backend_finished (backend);
}

/**
 * backend_remove_packages:
 */
static void
backend_remove_packages (PkBackend *backend, gchar **package_ids, gboolean allow_deps, gboolean autoremove)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_REMOVE);
	pk_backend_error_code (backend, PK_ERROR_ENUM_NO_NETWORK, "No network connection available");
	pk_backend_finished (backend);
}

static gboolean
backend_search_file_thread (PkBackend *backend)
{
	const gchar *search;
	PkBitfield filters;

	search = pk_backend_get_string (backend, "search");
	filters = (PkBitfield) pk_backend_get_uint (backend, "filters");

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);
	_cancel = false;
	pk_backend_set_allow_cancel (backend, true);

	// as we can only search for installed files lets avoid the opposite
	if (!pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_INSTALLED)) {
		aptcc *m_apt = new aptcc();
		if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
			egg_debug ("Failed to create apt cache");
			delete m_apt;
			return false;
		}

		vector<string> packages = search_file (backend, search, _cancel);
		for(vector<string>::iterator i = packages.begin();
		    i != packages.end(); ++i)
		{
			if (_cancel) {
			    break;
			}
			pkgCache::PkgIterator pkg = m_apt->cacheFile->FindPkg(i->c_str());
			pkgCache::VerIterator ver = m_apt->find_ver(pkg);
			if (ver.end() == true)
			{
				continue;
			}
			m_apt->emit_package(backend, filters, pkg, ver);
		}

		delete m_apt;
	}

	pk_backend_finished (backend);
	return true;
}

/**
 * backend_search_file:
 */
static void
backend_search_file (PkBackend *backend, PkBitfield filters, const gchar *search)
{
	pk_backend_thread_create (backend, backend_search_file_thread);
}

static gboolean
backend_search_group_thread (PkBackend *backend)
{
	const gchar *group;
	PkBitfield filters;

	group = pk_backend_get_string (backend, "search");
	filters = (PkBitfield) pk_backend_get_uint (backend, "filters");
	_cancel = false;
	pk_backend_set_allow_cancel (backend, true);

	if (group == NULL) {
		pk_backend_error_code (backend, PK_ERROR_ENUM_GROUP_NOT_FOUND, "Group is invalid.");
		pk_backend_finished (backend);
		return false;
	}

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);
	pk_backend_set_percentage (backend, 0);

	PkGroupEnum pkGroup = pk_group_enum_from_text (group);

	aptcc *m_apt = new aptcc();
	if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
		egg_debug ("Failed to create apt cache");
		delete m_apt;
		return false;
	}

	vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > output;
	for (pkgCache::PkgIterator pkg = m_apt->cacheFile->PkgBegin(); !pkg.end(); ++pkg) {
		if (_cancel) {
			break;
		}
		// Ignore packages that exist only due to dependencies.
		if (pkg.VersionList().end() && pkg.ProvidesList().end()) {
			continue;
		}

		// Ignore virtual packages
		pkgCache::VerIterator ver = m_apt->find_ver(pkg);
		if (ver.end() == false) {
			string section = pkg.VersionList().Section();

			size_t found;
			found = section.find_last_of("/");
			section = section.substr(found + 1);

			// Don't insert virtual packages instead add what it provides
			if (pkGroup == get_enum_group(section)) {
				output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(pkg, ver));
			}
		}
	}

	sort(output.begin(), output.end(), compare());

	// It's faster to emmit the packages here rather than in the matching part
	for(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator i=output.begin();
	    i != output.end(); ++i)
	{
		if (_cancel) {
			break;
		}
		m_apt->emit_package(backend, filters, i->first, i->second);
	}

	delete m_apt;

	pk_backend_set_percentage (backend, 100);
	pk_backend_finished (backend);
	return true;
}

/**
 * backend_search_group:
 */
static void
backend_search_group (PkBackend *backend, PkBitfield filters, const gchar *pkGroup)
{
	pk_backend_thread_create (backend, backend_search_group_thread);
}

static gboolean
backend_search_package_thread (PkBackend *backend)
{
	const gchar *search;
	PkBitfield filters;

	search = pk_backend_get_string (backend, "search");
	filters = (PkBitfield) pk_backend_get_uint (backend, "filters");
	
	pk_backend_set_percentage (backend, PK_BACKEND_PERCENTAGE_INVALID);
	_cancel = false;
	pk_backend_set_allow_cancel (backend, true);
	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);

	matcher *m_matcher = new matcher(string(search));
	if (m_matcher->hasError()) {
		egg_debug("Regex compilation error");
		delete m_matcher;
		return false;
	}

	aptcc *m_apt = new aptcc();
	if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
		egg_debug ("Failed to create apt cache");
		delete m_matcher;
		delete m_apt;
		return false;
	}

	if (_error->PendingError() == true)
	{
		delete m_matcher;
		delete m_apt;
		return false;
	}

	pkgDepCache::Policy Plcy;
	vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > output;
	if (pk_backend_get_bool (backend, "search_details")) {
		for (pkgCache::PkgIterator pkg = m_apt->cacheFile->PkgBegin(); !pkg.end(); ++pkg) {
			if (_cancel) {
				break;
			}
			// Ignore packages that exist only due to dependencies.
			if (pkg.VersionList().end() && pkg.ProvidesList().end()) {
				continue;
			}

			if (m_matcher->matches(pkg.Name())) {
				// Don't insert virtual packages instead add what it provides
				pkgCache::VerIterator ver = m_apt->find_ver(pkg);
				if (ver.end() == false) {
					output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(pkg, ver));
				} else {
					// iterate over the provides list
					for (pkgCache::PrvIterator Prv = pkg.ProvidesList(); Prv.end() == false; Prv++) {
						ver = m_apt->find_ver(Prv.OwnerPkg());

						// check to see if the provided package isn't virtual too
						if (ver.end() == false)
						{
							// we add the package now because we will need to
							// remove duplicates later anyway
							output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(Prv.OwnerPkg(), ver));
						}
					}
				}
			} else {
				// Don't insert virtual packages instead add what it provides
				pkgCache::VerIterator ver = m_apt->find_ver(pkg);
				if (ver.end() == false) {
					if (m_matcher->matches(get_default_short_description(ver, m_apt->packageRecords))
					    || m_matcher->matches(get_default_long_description(ver, m_apt->packageRecords))
					    || m_matcher->matches(get_short_description(ver, m_apt->packageRecords))
					    || m_matcher->matches(get_long_description(ver, m_apt->packageRecords)))
					{
						output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(pkg, ver));
					}
				} else {
					// iterate over the provides list
					for (pkgCache::PrvIterator Prv = pkg.ProvidesList(); Prv.end() == false; Prv++) {
						ver = m_apt->find_ver(Prv.OwnerPkg());

						// check to see if the provided package isn't virtual too
						if (ver.end() == false)
						{
							// we add the package now because we will need to
							// remove duplicates later anyway
							if (m_matcher->matches(Prv.OwnerPkg().Name())
							    || m_matcher->matches(get_default_short_description(ver, m_apt->packageRecords))
							    || m_matcher->matches(get_default_long_description(ver, m_apt->packageRecords))
							    || m_matcher->matches(get_short_description(ver, m_apt->packageRecords))
							    || m_matcher->matches(get_long_description(ver, m_apt->packageRecords)))
							{
								output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(Prv.OwnerPkg(), ver));
							}
						}
					}
				}
			}
		}
	} else {
		for (pkgCache::PkgIterator pkg = m_apt->cacheFile->PkgBegin(); !pkg.end(); ++pkg) {
			if (_cancel) {
				break;
			}
			// Ignore packages that exist only due to dependencies.
			if (pkg.VersionList().end() && pkg.ProvidesList().end()) {
				continue;
			}

			if (m_matcher->matches(pkg.Name())) {
				// Don't insert virtual packages instead add what it provides
				pkgCache::VerIterator ver = m_apt->find_ver(pkg);
				if (ver.end() == false) {
					output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(pkg, ver));
				} else {
					// iterate over the provides list
					for (pkgCache::PrvIterator Prv = pkg.ProvidesList(); Prv.end() == false; Prv++) {
						ver = m_apt->find_ver(Prv.OwnerPkg());

						// check to see if the provided package isn't virtual too
						if (ver.end() == false)
						{
							// we add the package now because we will need to
							// remove duplicates later anyway
							output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(Prv.OwnerPkg(), ver));
						}
					}
				}
			}
		}
	}

	sort(output.begin(), output.end(), compare());
	output.erase(unique(output.begin(), output.end(), result_equality()),
		       output.end());

	// It's faster to emmit the packages here than in the matching part
	for(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator i=output.begin();
	    i != output.end(); ++i)
	{
		if (_cancel) {
			break;
		}
		m_apt->emit_package(backend, filters, i->first, i->second);
	}

	delete m_matcher;
	delete m_apt;

	pk_backend_set_percentage (backend, 100);
	pk_backend_finished (backend);
	return true;
}

/**
 * backend_search_name:
 */
static void
backend_search_name (PkBackend *backend, PkBitfield filters, const gchar *search)
{
	pk_backend_set_bool (backend, "search_details", false);
	pk_backend_thread_create (backend, backend_search_package_thread);
}


/**
 * backend_search_details:
 */
static void
backend_search_details (PkBackend *backend, PkBitfield filters, const gchar *search)
{
	pk_backend_set_bool (backend, "search_details", true);
	pk_backend_thread_create (backend, backend_search_package_thread);
}

/**
 * backend_update_packages_update_timeout:
 **/
static gboolean
backend_update_packages_update_timeout (gpointer data)
{
	guint len;
	PkBackend *backend = (PkBackend *) data;
	const gchar *package;

	package = _package_ids[_package_current];
	/* emit the next package */
	if (egg_strequal (package, "powertop;1.8-1.fc8;i386;fedora")) {
		pk_backend_package (backend, PK_INFO_ENUM_UPDATING, package, "Power consumption monitor");
		_updated_powertop = true;
	}
	if (egg_strequal (package, "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed")) {
		pk_backend_package (backend, PK_INFO_ENUM_UPDATING, package,
				    "The Linux kernel (the core of the Linux operating system)");
		_updated_kernel = true;
	}
	if (egg_strequal (package, "gtkhtml2;2.19.1-4.fc8;i386;fedora")) {
		pk_backend_package (backend, PK_INFO_ENUM_UPDATING, package, "An HTML widget for GTK+ 2.0");
		_updated_gtkhtml = true;
	}

	/* are we done? */
	_package_current++;
	len = pk_package_ids_size (_package_ids);
	if (_package_current + 1 > len) {
		pk_backend_set_percentage (backend, 100);
		pk_backend_finished (backend);
		_signal_timeout = 0;
		return false;
	}
	return true;
}

/**
 * backend_update_packages_download_timeout:
 **/
static gboolean
backend_update_packages_download_timeout (gpointer data)
{
	guint len;
	PkBackend *backend = (PkBackend *) data;

	/* emit the next package */
	pk_backend_package (backend, PK_INFO_ENUM_DOWNLOADING, _package_ids[_package_current], "The same thing");

	/* are we done? */
	_package_current++;
	len = pk_package_ids_size (_package_ids);
	if (_package_current + 1 > len) {
		_package_current = 0;
		pk_backend_set_status (backend, PK_STATUS_ENUM_UPDATE);
		pk_backend_set_percentage (backend, 50);
		_signal_timeout = g_timeout_add (2000, backend_update_packages_update_timeout, backend);
		return false;
	}
	return true;
}

/**
 * backend_update_packages:
 */
static void
backend_update_packages (PkBackend *backend, gchar **package_ids)
{
	_package_ids = package_ids;
	_package_current = 0;
	pk_backend_set_percentage (backend, 0);
	pk_backend_set_status (backend, PK_STATUS_ENUM_DOWNLOAD);
	_signal_timeout = g_timeout_add (2000, backend_update_packages_download_timeout, backend);
}

static gboolean
backend_update_system_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;
	if (_progress_percentage == 100) {
		pk_backend_finished (backend);
		return false;
	}
	if (_progress_percentage == 0 && !_updated_powertop) {
		pk_backend_package (backend, PK_INFO_ENUM_DOWNLOADING,
				    "powertop;1.8-1.fc8;i386;fedora",
				    "Power consumption monitor");
	}
	if (_progress_percentage == 20 && !_updated_kernel) {
		pk_backend_package (backend, PK_INFO_ENUM_DOWNLOADING,
				    "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed",
				    "The Linux kernel (the core of the Linux operating system)");
	}
	if (_progress_percentage == 30 && !_updated_gtkhtml) {
		pk_backend_package (backend, PK_INFO_ENUM_BLOCKED,
				    "gtkhtml2;2.19.1-4.fc8;i386;fedora",
				    "An HTML widget for GTK+ 2.0");
		_updated_gtkhtml = false;
	}
	if (_progress_percentage == 40 && !_updated_powertop) {
		pk_backend_set_status (backend, PK_STATUS_ENUM_UPDATE);
		pk_backend_set_allow_cancel (backend, false);
		pk_backend_package (backend, PK_INFO_ENUM_INSTALLING,
				    "powertop;1.8-1.fc8;i386;fedora",
				    "Power consumption monitor");
		_updated_powertop = true;
	}
	if (_progress_percentage == 60 && !_updated_kernel) {
		pk_backend_package (backend, PK_INFO_ENUM_UPDATING,
				    "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed",
				    "The Linux kernel (the core of the Linux operating system)");
		_updated_kernel = true;
	}
	if (_progress_percentage == 80 && !_updated_kernel) {
		pk_backend_package (backend, PK_INFO_ENUM_CLEANUP,
				    "kernel;2.6.23-0.115.rc3.git1.fc8;i386;installed",
				    "The Linux kernel (the core of the Linux operating system)");
	}
	_progress_percentage += 10;
	pk_backend_set_percentage (backend, _progress_percentage);
	return true;
}

/**
 * backend_update_system:
 */
static void
backend_update_system (PkBackend *backend)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_DOWNLOAD);
	pk_backend_set_allow_cancel (backend, true);
	_progress_percentage = 0;
	pk_backend_require_restart (backend, PK_RESTART_ENUM_SYSTEM, NULL);
	_signal_timeout = g_timeout_add (1000, backend_update_system_timeout, backend);
}

/**
 * backend_get_repo_list:
 */
static void
backend_get_repo_list (PkBackend *backend, PkBitfield filters)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);
	pk_backend_repo_detail (backend, "fedora",
				"Fedora - 9", _repo_enabled_fedora);
	if (!pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_DEVELOPMENT)) {
		pk_backend_repo_detail (backend, "development",
					"Fedora - Development", _repo_enabled_devel);
	}
	pk_backend_repo_detail (backend, "livna-development",
				"Livna for Fedora Core 8 - i386 - Development Tree", _repo_enabled_livna);
	pk_backend_finished (backend);
}

/**
 * backend_repo_enable:
 */
static void
backend_repo_enable (PkBackend *backend, const gchar *rid, gboolean enabled)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_REQUEST);

	if (egg_strequal (rid, "local")) {
		egg_debug ("local repo: %i", enabled);
		_repo_enabled_local = enabled;
	} else if (egg_strequal (rid, "development")) {
		egg_debug ("devel repo: %i", enabled);
		_repo_enabled_devel = enabled;
	} else if (egg_strequal (rid, "fedora")) {
		egg_debug ("fedora repo: %i", enabled);
		_repo_enabled_fedora = enabled;
	} else if (egg_strequal (rid, "livna-development")) {
		egg_debug ("livna repo: %i", enabled);
		_repo_enabled_livna = enabled;
	} else {
		egg_warning ("unknown repo: %s", rid);
	}
	pk_backend_finished (backend);
}

/**
 * backend_repo_set_data:
 */
static void
backend_repo_set_data (PkBackend *backend, const gchar *rid, const gchar *parameter, const gchar *value)
{
	pk_backend_set_status (backend, PK_STATUS_ENUM_REQUEST);
	egg_warning ("REPO '%s' PARAMETER '%s' TO '%s'", rid, parameter, value);
	pk_backend_finished (backend);
}

/**
 * backend_what_provides_timeout:
 */
static gboolean
backend_what_provides_timeout (gpointer data)
{
	PkBackend *backend = (PkBackend *) data;
	if (_progress_percentage == 100) {
		if (egg_strequal (_search, "gstreamer0.10(decoder-audio/x-wma)(wmaversion=3)")) {
			pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE,
					    "gstreamer-plugins-bad;0.10.3-5.lvn;i386;available",
					    "GStreamer streaming media framework \"bad\" plug-ins");
		} else if (egg_strequal (_search, "gstreamer0.10(decoder-video/x-wma)(wmaversion=3)")) {
			pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE,
					    "gstreamer-plugins-flumpegdemux;0.10.15-5.lvn;i386;available",
					    "MPEG demuxer for GStreamer");
		} else {
			pk_backend_package (backend, PK_INFO_ENUM_INSTALLED,
					    "evince;0.9.3-5.fc8;i386;installed",
					    "PDF Document viewer");
			pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE,
					    "scribus;1.3.4-1.fc8;i386;fedora",
					    "Scribus is an desktop open source page layout program");
		}
		pk_backend_finished (backend);
		return false;
	}
	_progress_percentage += 10;
	pk_backend_set_percentage (backend, _progress_percentage);
	return true;
}

/**
 * backend_what_provides:
 */
static void
backend_what_provides (PkBackend *backend, PkBitfield filters, PkProvidesEnum provides, const gchar *search)
{
	_progress_percentage = 0;
	_search = search;
	_signal_timeout = g_timeout_add (200, backend_what_provides_timeout, backend);
	pk_backend_set_status (backend, PK_STATUS_ENUM_REQUEST);
	pk_backend_set_allow_cancel (backend, true);
	pk_backend_set_percentage (backend, _progress_percentage);
}

static gboolean
backend_get_packages_thread (PkBackend *backend)
{
	PkBitfield filters;
	filters = (PkBitfield) pk_backend_get_uint (backend, "filters");

	pk_backend_set_status (backend, PK_STATUS_ENUM_QUERY);

	_cancel = false;
	pk_backend_set_allow_cancel (backend, true);

	aptcc *m_apt = new aptcc();
	if (m_apt->init(pk_backend_get_locale (backend), *apt_source_list)) {
		egg_debug ("Failed to create apt cache");
		delete m_apt;
		return false;
	}

	vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > output;
	output.reserve(m_apt->cacheFile->HeaderP->PackageCount);
	for(pkgCache::PkgIterator pkg = m_apt->cacheFile->PkgBegin();
	    !pkg.end(); ++pkg)
	{
		if (_cancel) {
			break;
		}
		// Ignore packages that exist only due to dependencies.
		if(pkg.VersionList().end() && pkg.ProvidesList().end())
			continue;

		// Don't insert virtual packages as they don't have all kinds of info
		pkgCache::VerIterator ver = m_apt->find_ver(pkg);
		if (ver.end() == false) {
			output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(pkg, ver));
		}
	}

	sort(output.begin(), output.end(), compare());

	// It's faster to emmit the packages rather here than in the matching part
	for(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator i=output.begin();
	    i != output.end(); ++i)
	{
		if (_cancel) {
			break;
		}
		m_apt->emit_package(backend, filters, i->first, i->second);
	}

	delete m_apt;

	pk_backend_finished (backend);
	return true;
}

/**
  * backend_get_packages:
  */
static void
backend_get_packages (PkBackend *backend, PkBitfield filter)
{
	pk_backend_thread_create (backend, backend_get_packages_thread);
}

/**
 * backend_download_packages:
 */
static void
backend_download_packages (PkBackend *backend, gchar **package_ids, const gchar *directory)
{
	gchar *filename1;
	gchar *filename2;
	gchar *filelist;
	pk_backend_set_status (backend, PK_STATUS_ENUM_DOWNLOAD);

	filename1 = g_build_filename (directory, "powertop-1.8-1.fc8.rpm", NULL);
	g_file_set_contents (filename1, "hello dave", -1, NULL);
	pk_backend_package (backend, PK_INFO_ENUM_DOWNLOADING,
			    "powertop;1.8-1.fc8;i386;fedora", "Power consumption monitor");

	filename2 = g_build_filename (directory, "gtk2-2.11.6-6.fc8.rpm", NULL);
	g_file_set_contents (filename2, "hello brian", -1, NULL);
	pk_backend_package (backend, PK_INFO_ENUM_DOWNLOADING,
			    "gtk2;2.11.6-6.fc8;i386;fedora", "GTK+ Libraries for GIMP");

	/* send the filelist */
	filelist = g_strjoin (";", filename1, filename2, NULL);
	pk_backend_files (backend, NULL, filelist);

	g_free (filename1);
	g_free (filename2);
	g_free (filelist);

	pk_backend_finished (backend);
}

extern "C" PK_BACKEND_OPTIONS (
	"APTcc",					/* description */
	"Daniel Nicoletti <dantti85-pk@yahoo.com.br>",	/* author */
	backend_initialize,	        		/* initalize */
	backend_destroy,				/* destroy */
	backend_get_groups,				/* get_groups */
	backend_get_filters,				/* get_filters */
	backend_get_mime_types,				/* get_mime_types */
	backend_cancel,					/* cancel */
	backend_download_packages,			/* download_packages */
	NULL,						/* get_categories */
	backend_get_depends,				/* get_depends */
	backend_get_details,				/* get_details */
	NULL,						/* get_distro_upgrades */
	backend_get_files,				/* get_files */
	backend_get_packages,				/* get_packages */
	backend_get_repo_list,				/* get_repo_list */
	backend_get_requires,				/* get_requires */
	backend_get_update_detail,			/* get_update_detail */
	backend_get_updates,				/* get_updates */
	backend_install_files,				/* install_files */
	backend_install_packages,			/* install_packages */
	backend_install_signature,			/* install_signature */
	backend_refresh_cache,				/* refresh_cache */
	backend_remove_packages,			/* remove_packages */
	backend_repo_enable,				/* repo_enable */
	backend_repo_set_data,				/* repo_set_data */
	backend_resolve,				/* resolve */
	NULL,						/* rollback */
	backend_search_details,				/* search_details */
	backend_search_file,				/* search_file */
	backend_search_group,				/* search_group */
	backend_search_name,				/* search_name */
	backend_update_packages,			/* update_packages */
	backend_update_system,				/* update_system */
	backend_what_provides				/* what_provides */
);

