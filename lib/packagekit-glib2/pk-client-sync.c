/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <stdio.h>
#include <gio/gio.h>
#include <glib.h>
#include <packagekit-glib2/packagekit.h>

#include "egg-debug.h"

#include "pk-client-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	PkResults	*results;
} PkClientHelper;

/**
 * pk_client_generic_finish_sync:
 **/
static void
pk_client_generic_finish_sync (PkClient *client, GAsyncResult *res, PkClientHelper *helper)
{
	PkResults *results;
	results = pk_client_generic_finish (client, res, helper->error);
	if (results != NULL) {
		helper->results = g_object_ref (G_OBJECT(results));
		g_object_unref (results);
	}
	g_main_loop_quit (helper->loop);
}

/**
 * pk_client_resolve:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @packages: an array of package names to resolve, e.g. "gnome-system-tools"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Resolve a package name into a %package_id. This can return installed and
 * available packages and allows you find out if a package is installed locally
 * or is available in a repository.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_resolve (PkClient *client, PkBitfield filters, gchar **packages, GCancellable *cancellable,
		   PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_resolve_async (client, filters, packages, cancellable, progress_callback, progress_user_data,
				 (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_search_name:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @search: free text to search for, for instance, "power"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Search all the locally installed files and remote repositories for a package
 * that matches a specific name.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_search_name (PkClient *client, PkBitfield filters, const gchar *search, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_search_name_async (client, filters, search, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_search_details:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @search: free text to search for, for instance, "power"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Search all detailed summary information to try and find a keyword.
 * Think of this as pk_client_search_name(), but trying much harder and
 * taking longer.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_search_details (PkClient *client, PkBitfield filters, const gchar *search, GCancellable *cancellable,
			  PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_search_details_async (client, filters, search, cancellable, progress_callback, progress_user_data,
					(GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_search_group:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @search: a group enum to search for, for instance, "system-tools"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Return all packages in a specific group.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_search_group (PkClient *client, PkBitfield filters, const gchar *search, GCancellable *cancellable,
			PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_search_group_async (client, filters, search, cancellable, progress_callback, progress_user_data,
				      (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_search_file:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @search: file to search for, for instance, "/sbin/service"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Search for packages that provide a specific file.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_search_file (PkClient *client, PkBitfield filters, const gchar *search, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_search_file_async (client, filters, search, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_details:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get details of a package, so more information can be obtained for GUI
 * or command line tools.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_details (PkClient *client, gchar **package_ids, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_details_async (client, package_ids, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_update_detail:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get details about the specific update, for instance any CVE urls and
 * severity information.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_update_detail (PkClient *client, gchar **package_ids, GCancellable *cancellable,
			     PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_update_detail_async (client, package_ids, cancellable, progress_callback, progress_user_data,
					   (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_download_packages:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @directory: the location where packages are to be downloaded
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Downloads package files to a specified location.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_download_packages (PkClient *client, gchar **package_ids, const gchar *directory, GCancellable *cancellable,
			     PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_download_packages_async (client, package_ids, directory, cancellable, progress_callback, progress_user_data,
					   (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_updates:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_DEVEL or %PK_FILTER_ENUM_NONE
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get a list of all the packages that can be updated for all repositories.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_updates (PkClient *client, PkBitfield filters, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_updates_async (client, filters, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_old_transactions:
 * @client: a valid #PkClient instance
 * @number: the number of past transactions to return, or 0 for all
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get the old transaction list, mainly used for the rollback viewer.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_old_transactions (PkClient *client, guint number, GCancellable *cancellable,
			        PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_old_transactions_async (client, number, cancellable, progress_callback, progress_user_data,
					      (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_update_system:
 * @client: a valid #PkClient instance
 * @only_trusted: only trusted packages should be installed
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Update all the packages on the system with the highest versions found in all
 * repositories.
 * NOTE: you can't choose what repositories to update from, but you can do:
 * - pk_client_repo_disable()
 * - pk_client_update_system()
 * - pk_client_repo_enable()
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_update_system (PkClient *client, gboolean only_trusted, GCancellable *cancellable,
			 PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_update_system_async (client, only_trusted, cancellable, progress_callback, progress_user_data,
				       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_depends:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @recursive: If we should search recursively for depends
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get the packages that depend this one, i.e. child->parent.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_depends (PkClient *client, PkBitfield filters, gchar **package_ids, gboolean recursive, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_depends_async (client, filters, package_ids, recursive, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_packages:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get the list of packages from the backend
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_packages (PkClient *client, PkBitfield filters, GCancellable *cancellable,
		        PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_packages_async (client, filters, cancellable, progress_callback, progress_user_data,
				      (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_requires:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @recursive: If we should search recursively for requires
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get the packages that require this one, i.e. parent->child.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_requires (PkClient *client, PkBitfield filters, gchar **package_ids, gboolean recursive, GCancellable *cancellable,
		        PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_requires_async (client, filters, package_ids, recursive, cancellable, progress_callback, progress_user_data,
				      (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_what_provides:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_GUI | %PK_FILTER_ENUM_FREE or %PK_FILTER_ENUM_NONE
 * @provides: a #PkProvidesEnum value such as PK_PROVIDES_ENUM_CODEC
 * @search: a search term such as "sound/mp3"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * This should return packages that provide the supplied attributes.
 * This method is useful for finding out what package(s) provide a modalias
 * or GStreamer codec string.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_what_provides (PkClient *client, PkBitfield filters, PkProvidesEnum provides, const gchar *search, GCancellable *cancellable,
		         PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_what_provides_async (client, filters, provides, search, cancellable, progress_callback, progress_user_data,
				       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_distro_upgrades:
 * @client: a valid #PkClient instance
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * This method should return a list of distribution upgrades that are available.
 * It should not return updates, only major upgrades.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_distro_upgrades (PkClient *client, GCancellable *cancellable,
			       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_distro_upgrades_async (client, cancellable, progress_callback, progress_user_data,
					     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_files:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get the file list (i.e. a list of files installed) for the specified package.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_files (PkClient *client, gchar **package_ids, GCancellable *cancellable,
		     PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_files_async (client, package_ids, cancellable, progress_callback, progress_user_data,
				   (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_categories:
 * @client: a valid #PkClient instance
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get a list of all categories supported.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_categories (PkClient *client, GCancellable *cancellable,
			  PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_categories_async (client, cancellable, progress_callback, progress_user_data,
					(GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_remove_packages:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @allow_deps: if other dependant packages are allowed to be removed from the computer
 * @autoremove: if other packages installed at the same time should be tried to remove
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Remove a package (optionally with dependancies) from the system.
 * If %allow_deps is set to %FALSE, and other packages would have to be removed,
 * then the transaction would fail.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_remove_packages (PkClient *client, gchar **package_ids, gboolean allow_deps, gboolean autoremove, GCancellable *cancellable,
			   PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_remove_packages_async (client, package_ids, allow_deps, autoremove, cancellable, progress_callback, progress_user_data,
					 (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_refresh_cache:
 * @client: a valid #PkClient instance
 * @force: if we should aggressively drop caches
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Refresh the cache, i.e. download new metadata from a remote URL so that
 * package lists are up to date.
 * This action may take a few minutes and should be done when the session and
 * system are idle.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_refresh_cache (PkClient *client, gboolean force, GCancellable *cancellable,
			 PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_refresh_cache_async (client, force, cancellable, progress_callback, progress_user_data,
				       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_install_packages:
 * @client: a valid #PkClient instance
 * @only_trusted: only trusted packages should be installed
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Install a package of the newest and most correct version.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_install_packages (PkClient *client, gboolean only_trusted, gchar **package_ids, GCancellable *cancellable,
			    PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_install_packages_async (client, only_trusted, package_ids, cancellable, progress_callback, progress_user_data,
					  (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_install_signature:
 * @client: a valid #PkClient instance
 * @type: the signature type, e.g. %PK_SIGTYPE_ENUM_GPG
 * @key_id: a key ID such as "0df23df"
 * @package_id: a signature_id structure such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Install a software source signature of the newest and most correct version.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_install_signature (PkClient *client, PkSigTypeEnum type, const gchar *key_id, const gchar *package_id, GCancellable *cancellable,
			     PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_install_signature_async (client, type, key_id, package_id, cancellable, progress_callback, progress_user_data,
					   (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_update_packages:
 * @client: a valid #PkClient instance
 * @only_trusted: only trusted packages should be installed
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Update specific packages to the newest available versions.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_update_packages (PkClient *client, gboolean only_trusted, gchar **package_ids, GCancellable *cancellable,
			   PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_update_packages_async (client, only_trusted, package_ids, cancellable, progress_callback, progress_user_data,
					 (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_install_files:
 * @client: a valid #PkClient instance
 * @only_trusted: only trusted packages should be installed
 * @files: a file such as "/home/hughsie/Desktop/hal-devel-0.10.0.rpm"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Install a file locally, and get the deps from the repositories.
 * This is useful for double clicking on a .rpm or .deb file.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_install_files (PkClient *client, gboolean only_trusted, gchar **files, GCancellable *cancellable,
			 PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_install_files_async (client, only_trusted, files, cancellable, progress_callback, progress_user_data,
				       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_accept_eula:
 * @client: a valid #PkClient instance
 * @eula_id: the <literal>eula_id</literal> we are agreeing to
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * We may want to agree to a EULA dialog if one is presented.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_accept_eula (PkClient *client, const gchar *eula_id, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_accept_eula_async (client, eula_id, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_rollback:
 * @client: a valid #PkClient instance
 * @transaction_id: the <literal>transaction_id</literal> we want to return to
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * We may want to agree to a EULA dialog if one is presented.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_rollback (PkClient *client, const gchar *transaction_id, GCancellable *cancellable,
		    PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_accept_eula_async (client, transaction_id, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_get_repo_list:
 * @client: a valid #PkClient instance
 * @filters: a %PkBitfield such as %PK_FILTER_ENUM_DEVEL or %PK_FILTER_ENUM_NONE
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Get the list of repositories installed on the system.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_get_repo_list (PkClient *client, PkBitfield filters, GCancellable *cancellable,
			 PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_get_repo_list_async (client, filters, cancellable, progress_callback, progress_user_data,
				       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_repo_enable:
 * @client: a valid #PkClient instance
 * @repo_id: a repo_id structure such as "livna-devel"
 * @enabled: if we should enable the repository
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Enable or disable the repository.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_repo_enable (PkClient *client, const gchar *repo_id, gboolean enabled, GCancellable *cancellable,
		       PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_repo_enable_async (client, repo_id, enabled, cancellable, progress_callback, progress_user_data,
				     (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_repo_set_data:
 * @client: a valid #PkClient instance
 * @repo_id: a repo_id structure such as "livna-devel"
 * @parameter: the parameter to change
 * @value: what we should change it to
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * We may want to set a repository parameter.
 * NOTE: this is free text, and is left to the backend to define a format.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_repo_set_data (PkClient *client, const gchar *repo_id, const gchar *parameter, const gchar *value, GCancellable *cancellable,
			 PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_repo_set_data_async (client, repo_id, parameter, value, cancellable, progress_callback, progress_user_data,
				       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_simulate_install_files:
 * @client: a valid #PkClient instance
 * @files: a file such as "/home/hughsie/Desktop/hal-devel-0.10.0.rpm"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Simulate an installation of files.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_simulate_install_files (PkClient *client, gchar **files, GCancellable *cancellable,
				  PkProgressCallback progress_callback, gpointer progress_user_data,
				  GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_simulate_install_files_async (client, files, cancellable, progress_callback, progress_user_data,
						(GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_simulate_install_packages:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Simulate an installation of packages.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_simulate_install_packages (PkClient *client, gchar **package_ids, GCancellable *cancellable,
				     PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_simulate_install_packages_async (client, package_ids, cancellable, progress_callback, progress_user_data,
						   (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_simulate_remove_packages:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Simulate a removal of packages.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_simulate_remove_packages (PkClient *client, gchar **package_ids, GCancellable *cancellable,
				    PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_simulate_remove_packages_async (client, package_ids, cancellable, progress_callback, progress_user_data,
						  (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_simulate_update_packages:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Simulate an update of packages.
 *
 * Warning: this function is synchronous, and may block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_simulate_update_packages (PkClient *client, gchar **package_ids, GCancellable *cancellable,
				    PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_simulate_update_packages_async (client, package_ids, cancellable, progress_callback, progress_user_data,
						  (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

/**
 * pk_client_adopt:
 * @client: a valid #PkClient instance
 * @package_ids: a null terminated array of package_id structures such as "hal;0.0.1;i386;fedora"
 * @cancellable: a #GCancellable or %NULL
 * @progress_callback: the function to run when the progress changes
 * @progress_user_data: data to pass to @progress_callback
 * @error: the #GError to store any failure, or %NULL
 *
 * Adopt a transaction.
 *
 * Warning: this function is synchronous, and will block. Do not use it in GUI
 * applications.
 *
 * Return value: a %PkResults object, or NULL for error
 **/
PkResults *
pk_client_adopt (PkClient *client, const gchar *transaction_id, GCancellable *cancellable,
		 PkProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	PkClientHelper *helper;
	PkResults *results;

	g_return_val_if_fail (PK_IS_CLIENT (client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper = g_new0 (PkClientHelper, 1);
	helper->loop = g_main_loop_new (NULL, FALSE);
	helper->error = error;

	/* run async method */
	pk_client_adopt_async (client, transaction_id, cancellable, progress_callback, progress_user_data,
			       (GAsyncReadyCallback) pk_client_generic_finish_sync, helper);

	g_main_loop_run (helper->loop);

	results = helper->results;

	/* free temp object */
	g_main_loop_unref (helper->loop);
	g_free (helper);

	return results;
}

