<?php

/**
 * @file
 * Admin page callbacks for the Skinr module.
 */

/**
 * Menu callback; Updates the skin configuration's status.
 */
function skinr_ui_skin_status_set($skin, $status) {
  // We require a token in the query string to prevent CSFR attacks.
  if (!isset($_GET['token']) || !drupal_valid_token($_GET['token'], 'admin/structure/skinr/skin/' . $skin->sid . '/' . ($status ? 'enable' : 'disable'))) {
    return MENU_ACCESS_DENIED;
  }

  // Load an uncached version of the skin configuration object.
  $skin = skinr_skin_load_unchanged($skin->sid);
  // Let's save some time in skinr_skin_save() by setting $skin->original here.
  $skin->original = clone($skin);

  // Update the status and save the skin configuration.
  $skin->status = $status ? 1 : 0;
  skinr_skin_save($skin);
  if ($status) {
    drupal_set_message(t('Skin configuration has been enabled.'));
  }
  else {
    drupal_set_message(t('Skin configuration has been disabled.'));
  }

  // Return to the skin configuration overview page.
  drupal_goto('admin/structure/skinr');
}

/**
 * Implements hook_skinr_ui_operations().
 */
function skinr_ui_skinr_ui_operations() {
  $operations = array(
    'enable' => array(
      'label' => t('Enable selected skin configuration'),
      'callback' => 'skinr_ui_mass_update',
      'callback arguments' => array('updates' => array('status' => 1)),
    ),
    'disable' => array(
      'label' => t('Disable selected skin configuration'),
      'callback' => 'skinr_ui_mass_update',
      'callback arguments' => array('updates' => array('status' => 0)),
    ),
    'delete' => array(
      'label' => t('Delete selected skin configuration'),
      'callback' => NULL,
    ),
  );
  return $operations;
}

/**
 * List skinr administration filters that can be applied.
 *
 * @return
 *   An array of filters.
 */
function skinr_ui_filters() {
  // Theme filter.
  $themes = list_themes();
  ksort($themes);

  $options = array('[any]' => t('any'));
  foreach ($themes as $theme) {
    if (!$theme->status) {
      continue;
    }
    $options[$theme->name] = $theme->info['name'];
  }

  $filters['theme'] = array(
    'title' => t('theme'),
    'options' => $options,
  );

  // Type filter.
  $config = skinr_get_config_info();

  $options = array('[any]' => t('any'));
  foreach ($config as $type) {
    $options[$type] = $type;
  }

  $filters['module'] = array(
    'title' => t('type'),
    'options' => $options,
  );

  // Skin filter.
  $skin_info = skinr_get_skin_info();

  $options = array('[any]' => t('any'), '_additional' => t('Additional classes'));
  foreach ($skin_info as $skin => $info) {
    $options[$skin] = $info['title'];
  }

  $filters['skin'] = array(
    'title' => t('skin'),
    'options' => $options,
  );

  // Status filter.
  $filters['status'] = array(
    'title' => t('status'),
    'options' => array(
      '[any]' => t('any'),
      '1' => t('enabled'),
      '0' => t('disabled'),
    ),
  );

  return $filters;
}

/**
 * Apply filters for skin configuration administration filters based on session.
 *
 * @param $query
 *   A SelectQuery to which the filters should be applied.
 */
function skinr_ui_build_filter_query(SelectQueryInterface $query) {
  // Build query
  $filter_data = isset($_SESSION['skinr_ui_overview_filter']) ? $_SESSION['skinr_ui_overview_filter'] : array();
  foreach ($filter_data as $index => $filter) {
    list($key, $value) = $filter;
    $query->condition('s.' . $key, $value);
  }
}

/**
 * Form builder for the Skinr administration filters form.
 *
 * @ingroup forms
 */
function skinr_ui_filter_form() {
  $session = &$_SESSION['skinr_ui_overview_filter'];
  $session = is_array($session) ? $session : array();
  $filters = skinr_ui_filters();

  $i = 0;
  $form['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Show only items where'),
    '#theme' => 'exposed_filters__skinr',
  );
  foreach ($session as $filter) {
    list($type, $value) = $filter;
    $value = $filters[$type]['options'][$value];
    $t_args = array('%property' => $filters[$type]['title'], '%value' => $value);

    if ($i++) {
      $form['filters']['current'][] = array('#markup' => t('and where %property is %value', $t_args));
    }
    else {
      $form['filters']['current'][] = array('#markup' => t('where %property is %value', $t_args));
    }
    // Remove the option if it is already being filtered on.
    unset($filters[$type]);
  }

  $form['filters']['status'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('clearfix')),
    '#prefix' => ($i ? '<div class="additional-filters">' . t('and where') . '</div>' : ''),
  );
  $form['filters']['status']['filters'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('filters')),
   );

  foreach ($filters as $key => $filter) {
    $names[$key] = $filter['title'];
    $form['filters']['status']['filters'][$key] = array(
      '#type' => 'select',
      '#title' => $filter['title'],
      '#options' => $filter['options'],
      '#default_value' => '[any]',
    );
  }

  $form['filters']['status']['actions'] = array(
    '#type' => 'actions',
    '#id' => 'skinr-exposed-filters',
    '#attributes' => array('class' => array('container-inline')),
  );
  if (count($filters)) {
    $form['filters']['status']['actions']['submit'] = array(
      '#type' => 'submit',
      '#value' => count($session) ? t('Refine') : t('Filter'),
    );
  }
  if (count($session)) {
    $form['filters']['status']['actions']['undo'] = array('#type' => 'submit', '#value' => t('Undo'));
    $form['filters']['status']['actions']['reset'] = array('#type' => 'submit', '#value' => t('Reset'));
  }

  drupal_add_js('misc/form.js');

  return $form;
}

/**
 * Mass update skin configurations, updating all skin configurations in the
 * $skins array with the field values in $updates.
 *
 * IMPORTANT NOTE: This function is intended to work when called
 * from a form submit handler. Calling it outside of the form submission
 * process may not work correctly.
 *
 * @param array $skins
 *   Array of skin configuration sids to update.
 * @param array $updates
 *   Array of key/value pairs with skin configuration field names and the
 *   value to update that field to.
 */
function skinr_ui_mass_update($skins, $updates) {
  // We use batch processing to prevent timeout when updating a large number
  // of skins.
  if (count($skins) > 10) {
    $batch = array(
      'operations' => array(
        array('_skinr_ui_mass_update_batch_process', array($skins, $updates))
      ),
      'finished' => '_skinr_ui_mass_update_batch_finished',
      'title' => t('Processing'),
      // We use a single multi-pass operation, so the default
      // 'Remaining x of y operations' message will be confusing here.
      'progress_message' => '',
      'error_message' => t('The update has encountered an error.'),
      // The operations do not live in the .module file, so we need to
      // tell the batch engine which file to load before calling them.
      'file' => drupal_get_path('module', 'skinr_ui') . '/skinr_ui.admin.inc',
    );
    batch_set($batch);
  }
  else {
    foreach ($skins as $sid) {
      _skinr_ui_mass_update_helper($sid, $updates);
    }
    drupal_set_message(t('The update has been performed.'));
  }
}

/**
 * Helper function for skin configuration mass updates.
 */
function _skinr_ui_mass_update_helper($sid, $updates) {
  drupal_static_reset('skinr_skin_load_multiple');
  $skin = skinr_skin_load($sid);
  // For efficiency manually store the original skin configuration before
  // applying any changes.
  $skin->original = clone $skin;
  foreach ($updates as $name => $value) {
    $skin->$name = $value;
  }
  skinr_skin_save($skin);
  return $skin;
}

/**
 * Batch operation for skin configuration mass updates.
 */
function _skinr_ui_mass_update_batch_process($skins, $updates, &$context) {
  if (!isset($context['sandbox']['progress'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = count($skins);
    $context['sandbox']['skins'] = $skins;
  }

  // Process skins in groups of 5.
  $count = min(5, count($context['sandbox']['skins']));
  for ($i = 1; $i <= $count; $i++) {
    // For each sid, load the skin configuration, reset the values, and save it.
    $sid = array_shift($context['sandbox']['skins']);
    $skin = _skinr_ui_mass_update_helper($sid, $updates);

    // Store result for post-processing in the finished callback.
    // @todo
    $context['results'][] = l($skin->title, 'node/' . $skin->sid);

    // Update our progress information.
    $context['sandbox']['progress']++;
  }

  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * Batch 'finished' callback for skin configuration mass updates.
 */
function _skinr_ui_mass_update_batch_finished($success, $results, $operations) {
  if ($success) {
    drupal_set_message(t('The update has been performed.'));
  }
  else {
    drupal_set_message(t('An error occurred and processing did not complete.'), 'error');
    $message = format_plural(count($results), '1 item successfully processed:', '@count items successfully processed:');
    $message .= theme('item_list', array('items' => $results));
    drupal_set_message($message);
  }
}

/**
 * Form submission handler for skinr_ui_filter_form().
 */
function skinr_ui_filter_form_submit($form, &$form_state) {
  $filters = skinr_ui_filters();
  switch ($form_state['values']['op']) {
    case t('Filter'):
    case t('Refine'):
      // Apply every filter that has a choice selected other than 'any'.
      foreach ($filters as $filter => $options) {
        if (isset($form_state['values'][$filter]) && $form_state['values'][$filter] != '[any]') {
          // Flatten the options array to accommodate hierarchical/nested options.
          $flat_options = form_options_flatten($filters[$filter]['options']);
          // Only accept valid selections offered on the dropdown, block bad input.
          if (isset($flat_options[$form_state['values'][$filter]])) {
            $_SESSION['skinr_ui_overview_filter'][] = array($filter, $form_state['values'][$filter]);
          }
        }
      }
      break;
    case t('Undo'):
      array_pop($_SESSION['skinr_ui_overview_filter']);
      break;
    case t('Reset'):
      $_SESSION['skinr_ui_overview_filter'] = array();
      break;
  }
}

/**
 * Menu callback: skin configurations administration.
 *
 * @ingroup forms
 */
function skinr_ui_list($form, &$form_state) {
  if (isset($form_state['values']['operation']) && $form_state['values']['operation'] == 'delete') {
    return skinr_ui_multiple_delete_confirm($form, $form_state, array_filter($form_state['values']['skins']));
  }
  $form['filter'] = skinr_ui_filter_form();
  $form['#submit'][] = 'skinr_ui_filter_form_submit';
  $form['admin'] = skinr_ui_admin_skins();

  return $form;
}

/**
 * Form builder: Builds the skin configuration administration overview.
 *
 * @ingroup forms
 */
function skinr_ui_admin_skins() {
  // Build the 'Update options' form.
  $form['options'] = array(
    '#type' => 'fieldset',
    '#title' => t('Update options'),
    '#attributes' => array('class' => array('container-inline')),
  );
  $options = array();
  foreach (module_invoke_all('skinr_ui_operations') as $operation => $array) {
    $options[$operation] = $array['label'];
  }
  $form['options']['operation'] = array(
    '#type' => 'select',
    '#title' => t('Operation'),
    '#title_display' => 'invisible',
    '#options' => $options,
    '#default_value' => 'enable',
  );
  $form['options']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Update'),
    '#validate' => array('skinr_ui_admin_skins_validate'),
    '#submit' => array('skinr_ui_admin_skins_submit'),
  );

  $header = array(
    'theme' => array('data' => t('Theme'), 'field' => 's.theme'),
    'type' => array('data' => t('Type'), 'field' => 's.module'),
    'element' => array('data' => t('Element ID'), 'field' => 's.element'),
    'skin' => array('data' => t('Skin'), 'field' => 's.skin'),
    'status' => array('data' => t('Status'), 'field' => 's.status'),
    'operations' => array('data' => t('Operations')),
  );

  $query = db_select('skinr_skins', 's')->extend('PagerDefault')->extend('TableSort');
  skinr_ui_build_filter_query($query);

  $sids = $query
    ->fields('s', array('sid'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute()
    ->fetchCol();
  $skins = skinr_skin_load_multiple($sids);

  $themes = list_themes();
  $skin_info = skinr_get_skin_info();
  $destination = drupal_get_destination();
  $options = array();
  foreach ($skins as $skin) {
    $operations = array(
      'edit' => array(
        'title' => t('edit'),
        'href' => 'admin/structure/skinr/edit/nojs/' . $skin->module . '/' . $skin->element,
        'query' => $destination,
      ),
      'status' => array(
        'title' => $skin->status ? t('disable') : t('enable'),
        'href' => 'admin/structure/skinr/skin/' . $skin->sid . '/' . ($skin->status ? 'disable' : 'enable'),
        'query' => $destination + array(
          'token' => drupal_get_token('admin/structure/skinr/skin/' . $skin->sid . '/' . ($skin->status ? 'disable' : 'enable')),
        ),
      ),
      'delete' => array(
        'title' => t('delete'),
        'href' => 'admin/structure/skinr/skin/' . $skin->sid . '/delete',
        'query' => $destination,
      ),
    );
    $options[$skin->sid] = array(
      'theme' => isset($themes[$skin->theme]) ? $themes[$skin->theme]->info['name'] : '<em>' . $skin->theme . '</em>',
      'type' => $skin->module,
      'element' => $skin->element,
      'skin' => $skin->skin == '_additional' ? '<em>' . t('Additional classes') . '</em>' : (isset($skin_info[$skin->skin]) ? $skin_info[$skin->skin]['title'] : '<em>' . $skin->skin . '</em>'),
      'status' => $skin->status ? t('enabled') : t('disabled'),
      'operations' => array(
        'data' => array(
          '#theme' => 'links__skinr_ui_operations',
          '#links' => $operations,
          '#attributes' => array('class' => array('links', 'inline')),
        ),
      ),
    );
  }

  $form['skins'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $options,
    '#empty' => t('No content available.'),
  );

  $form['pager'] = array('#markup' => theme('pager'));
  return $form;
}

/**
 * Form validation handler for skinr_ui_list().
 *
 * Check if any skinr settings have been selected to perform the chosen
 * 'Update option' on.
 */
function skinr_ui_admin_skins_validate($form, &$form_state) {
  // Error if there are no items to select.
  if (!is_array($form_state['values']['skins']) || !count(array_filter($form_state['values']['skins']))) {
    form_set_error('', t('No items selected.'));
  }
}

/**
 * Form submission handler for skinr_ui_list().
 *
 * Execute the chosen 'Update option' on the selected skinr settings.
 */
function skinr_ui_admin_skins_submit($form, &$form_state) {
  $operations = module_invoke_all('skinr_ui_operations');
  $operation = $operations[$form_state['values']['operation']];
  // Filter out unchecked nodes
  $nodes = array_filter($form_state['values']['skins']);
  if ($function = $operation['callback']) {
    // Add in callback arguments if present.
    if (isset($operation['callback arguments'])) {
      $args = array_merge(array($nodes), $operation['callback arguments']);
    }
    else {
      $args = array($nodes);
    }
    call_user_func_array($function, $args);

    cache_clear_all();
  }
  else {
    // We need to rebuild the form to go to a second step. For example, to
    // show the confirmation form for the deletion of nodes.
    $form_state['rebuild'] = TRUE;
  }
}

/**
 * Form builder for the confirmation form when deleting multiple Skinr settings.
 *
 * @param $skins
 *   An array of skins to delete.
 *
 * @ingroup forms
 */
function skinr_ui_multiple_delete_confirm($form, &$form_state, $skins) {
  $themes = list_themes();

  $form['skins'] = array('#prefix' => '<ul>', '#suffix' => '</ul>', '#tree' => TRUE);
  // array_filter returns only elements with TRUE values
  $original_skins = skinr_skin_load_multiple(array_keys($skins));
  foreach ($skins as $sid => $value) {
    $form['skins'][$sid] = array(
      '#type' => 'hidden',
      '#value' => $sid,
      '#prefix' => '<li>',
      '#suffix' => t('Skin %skin for element %element of type %type for the %theme theme', array('%skin' => $original_skins[$sid]->skin, '%element' => $original_skins[$sid]->element, '%type' => $original_skins[$sid]->module, '%theme' => $themes[$original_skins[$sid]->theme]->info['name'])) ."</li>\n",
    );
  }
  $form['operation'] = array('#type' => 'hidden', '#value' => 'delete');
  $form['#submit'][] = 'skinr_ui_multiple_delete_confirm_submit';
  $confirm_question = format_plural(count($skins),
                                  'Are you sure you want to delete this item?',
                                  'Are you sure you want to delete these items?');
  return confirm_form($form,
                    $confirm_question,
                    'admin/structure/skinr', t('This action cannot be undone.'),
                    t('Delete'), t('Cancel'));
}

/**
 * Form submission handler for skinr_ui_multiple_delete_confirm().
 */
function skinr_ui_multiple_delete_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    skinr_skin_delete_multiple(array_keys($form_state['values']['skins']));
    $count = count($form_state['values']['skins']);
    if ($count == 1) {
      watchdog('content', 'Deleted 1 skin configuration.');
    }
    else {
      watchdog('content', 'Deleted @count skin configurations.', array('@count' => $count));
    }
    drupal_set_message(format_plural($count, 'Deleted 1 skin configuration.', 'Deleted @count skin configurations.'));
  }
  $form_state['redirect'] = 'admin/structure/skinr';
}

/**
 * Menu callback for admin/structure/block.
 *
 * @param $theme
 *   The theme to display the administration page for. If not provided, defaults
 *   to the currently used theme.
 */
function skinr_ui_admin_library($theme = NULL) {
  global $theme_key;

  drupal_theme_initialize();

  if (!isset($theme)) {
    // If theme is not specifically set, rehash for the current theme.
    $theme = $theme_key;
  }

  return drupal_get_form('skinr_ui_admin_library_form', $theme);
}

/**
 * Menu callback: skins administration.
 */
function skinr_ui_admin_library_form($form, $form_state, $theme) {
  $form['edited_theme'] = array(
    '#type' => 'value',
    '#value' => $theme,
  );

  $skins = skinr_get_skin_info();
  if (empty($skins)) {
    $form['skins_empty'] = array(
      '#markup' => t("You don't have any skins to manage."),
    );
    return $form;
  }

  if (isset($form_state['storage']['sids'])) {
    // Ask for confirmation for disabling skin configurations.
    foreach ($form_state['storage']['skins'] as $skin) {
      $items[] = $skins[$skin]['title'];
    }
    $message = t('Would you like to disable all skin configurations for the selected skins?') . theme('item_list', array('items' => $items));
    // Insert a confirmation form.
    return confirm_form(
      $form,
      t('Disable all skin configurations for selected skins?'),
      'admin/structure/skinr/library',
      $message,
      t('Yes'),
      t('No')
    );
  }

  // Apply overridden status.
  foreach ($skins as $name => $skin_info) {
    $skins[$name]['status'] = skinr_skin_info_status_get($skins[$name]);
  }

  $groups = skinr_get_group_info();

  uasort($skins, 'skinr_ui_sort_by_title');
  $form['skins'] = array('#tree' => TRUE);

  // Iterate through each of the skins.
  foreach ($skins as $name => $skin_info) {
    $extra = array();

    // Set status.
    $extra['enabled'] = $skin_info['status'][$theme];

    // Create a row entry for this skin.
    $group = $groups[$skin_info['group']]['title'];
    $form['skins'][$group][$name] = _skinr_ui_admin_library_form_build_row($skin_info, $extra, $theme);
  }

  // Add basic information to the fieldsets.
  foreach (element_children($form['skins']) as $package) {
    $form['skins'][$package] += array(
      '#type' => 'fieldset',
      '#title' => $package,
      '#collapsible' => TRUE,
      '#theme' => 'skinr_ui_admin_library_fieldset',
      '#header' => array(
        array('data' => t('Enabled'), 'class' => array('checkbox')),
        t('Name'),
        t('Source'),
        t('Version'),
        t('Theme hooks'),
      ),
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );
  $form['actions']['reset'] = array(
    '#type' => 'submit',
    '#value' => t('Reset to defaults'),
  );
  $form['#action'] = url('admin/structure/skinr/library');

  return $form;
}

/**
 * Build a table row for the skin info listing page.
 */
function _skinr_ui_admin_library_form_build_row($skin_info, $extra, $theme) {
  // Add in the defaults.
  $extra += array(
    'enabled' => FALSE,
    'disabled' => FALSE,
    'links' => array(),
  );
  $form = array(
    '#tree' => TRUE,
  );

  $form['name'] = array(
    '#markup' => $skin_info['title'],
  );
  $form['description'] = array(
    '#markup' => t($skin_info['description']),
  );

  // Grab source info.
  $info = system_get_info($skin_info['source']['type'], $skin_info['source']['name']);
  $source = !empty($info['name']) ? $info['name'] : $skin_info['source']['name'];

  $form['source'] = array(
    '#markup' => t('%source !type', array('%source' => $source, '!type' => $skin_info['source']['type'] == 'module' ? t('module') : t('theme'))),
  );
  $form['version'] = array(
    '#markup' => $skin_info['source']['version'],
  );

  $theme_hooks = array();
  foreach ($skin_info['theme hooks'] as $theme_hook) {
    $theme_hooks[] = $theme_hook == '*' ? t('all hooks') : $theme_hook;
  }
  $form['theme hooks'] = array(
    '#markup' => implode('<br />', $theme_hooks),
  );

  $form['enable'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable'),
    '#default_value' => $extra['enabled'],
  );
  if ($extra['disabled']) {
    $form['enable']['#disabled'] = TRUE;
  }

  return $form;
}

/**
 * Sort skin infos by title.
 *
 * @param $a
 *   The first skin info to compare.
 * @param $b
 *   The second skin info to compare.
 *
 * @return
 *   Returns < 0 if $a is less than $b; > 0 if $a is greater than $b, and 0
 *   if they are equal.
 */
function skinr_ui_sort_by_title($a, $b) {
  return strcasecmp($a['title'], $b['title']);
}

/**
 * Form submission handler for skinr_ui_admin_library_form().
 */
function skinr_ui_admin_library_form_submit($form, &$form_state) {
  $skins = skinr_get_skin_info();
  $theme = $form_state['values']['edited_theme'];
  $theme_info = system_get_info('theme', $theme);
  $reset = $form_state['clicked_button']['#id'] == 'edit-reset' ? TRUE : FALSE;

  if ($reset) {
    // Reset all values to their default.
    foreach ($form_state['values']['skins'] as $category => $data) {
      foreach ($data as $skin => $enabled) {
        $default_status = isset($skins[$skin]['status'][$theme]) ? $skins[$skin]['status'][$theme] : $skins[$skin]['default status'];
        $form_state['values']['skins'][$category][$skin]['enable'] = $default_status;
      }
    }
  }

  if ($form_state['clicked_button']['#id'] == 'edit-submit' || $reset) {
    // Make sure we don't disable skins for which configuration exists. Ask to
    // disable all related skin configurations so we can disable the skin.

    $affected_skins = array();
    $disable_sids = array();
    $rebuild = FALSE;

    foreach ($form_state['values']['skins'] as $category => $data) {
      foreach ($data as $skin => $enabled) {
        $enabled = $enabled['enable'];
        $status = skinr_skin_info_status_get($skins[$skin]);

        if (!empty($status[$theme]) && !$enabled) {
          // This skin is being disabled.
          $affected_skins[] = $skin;

          // Find all enabled configurations for this skin.
          $params = array(
            'theme' => $theme,
            'skin' => $skin,
            'status' => 1,
          );
          $sids = skinr_skin_get_sids($params);
          if (count($sids)) {
            $disable_sids += $sids;
            $rebuild = TRUE;
          }
        }
      }
    }

    if ($rebuild) {
      $form_state['storage']['status'] = $form_state['values']['skins'];
      $form_state['storage']['skins'] = $affected_skins;
      $form_state['storage']['sids'] = $disable_sids;
      $form_state['storage']['reset'] = $reset;
      $form_state['rebuild'] = TRUE;
      return;
    }
  }

  if (!empty($form_state['storage']['sids'])) {
    // Disable any configurations for skins that are being disabled.
    db_update('skinr_skins')
      ->fields(array('status' => 0))
      ->condition('sid', $form_state['storage']['sids'])
      ->execute();

    // Clear skinr_skin_load_multiple cache.
    drupal_static_reset('skinr_skin_load_multiple');

    foreach ($form_state['storage']['skins'] as $skin) {
      drupal_set_message(t('Disabled all skin configurations for skin %skin and theme %theme.', array('%skin' => $skin, '%theme' => $theme_info['name'])));
    }
    $changed_status = $form_state['storage']['status'];
    $reset = $form_state['storage']['reset'];
  }
  else {
    $changed_status = $form_state['values']['skins'];
  }

  // Save new status.
  foreach ($changed_status as $category => $data) {
    foreach ($data as $skin => $enabled) {
      $enabled = $enabled['enable'];
      $status = skinr_skin_info_status_get($skins[$skin]);
      if (!isset($status[$theme]) || $status[$theme] != $enabled) {
        // Update status.
        $status[$theme] = $enabled;
        skinr_skin_info_status_set($skins[$skin], $status);
      }
    }
  }

  if ($reset) {
    drupal_set_message(t("Statuses for %theme's skins have been reset to their defaults.", array('%theme' => $theme_info['name'])));
  }
  else {
    drupal_set_message(t("Statuses for %theme's skins have been updated.", array('%theme' => $theme_info['name'])));
  }
  $form_state['redirect'] = 'admin/structure/skinr/library';
}

/**
 * Returns HTML for the skin info listing form.
 *
 * @param $variables
 *   An associative array containing:
 *   - form: A render element representing the form.
 *
 * @ingroup themeable
 */
function theme_skinr_ui_admin_library_fieldset($variables) {
  $form = $variables['form'];

  // Individual table headers.
  $rows = array();
  // Iterate through all the modules, which are
  // children of this fieldset.
  foreach (element_children($form) as $skin_name) {
    // Stick it into $skinset for easier accessing.
    $skin_info = $form[$skin_name];

    $row = array();

    // Enabled.
    unset($skin_info['enable']['#title']);
    $row[] = array('class' => array('checkbox'), 'data' => drupal_render($skin_info['enable']));

    // Name.
    $row[] = theme('skinr_ui_admin_library_summary', array(
      'name' => drupal_render($skin_info['name']),
      'description' => drupal_render($skin_info['description']),
    ));

    // Source.
    $row[] = drupal_render($skin_info['source']);
    $row[] = drupal_render($skin_info['version']);

    // Theme hooks.
    $row[] = drupal_render($skin_info['theme hooks']);

    $rows[] = $row;
  }

  return theme('table', array('header' => $form['#header'], 'rows' => $rows));
}

/**
 * Returns HTML for a message about incompatible skinsets.
 *
 * @param $variables
 *   An associative array containing:
 *   - message: The form array representing the currently disabled modules.
 *
 * @ingroup themeable
 */
function theme_skinr_ui_admin_library_summary($variables) {
  return '<div class="skin-infos-summary"><h2>'. $variables['name'] .'</h2><div class="description">'. $variables['description'] .'</div></div>';
}

/**
 * Form builder for the Skinr settings export form.
 *
 * @param $theme
 *   (optional) The name of the theme to export Skinr settings for. If no
 *   theme name is provided a theme selector is shown.
 *
 * @ingroup forms
 */
function skinr_ui_export_form($form, &$form_state, $theme = NULL) {
  $form = array();
  $themes = list_themes();

  if ($theme) {
    // Export an individual theme.
    $theme = str_replace('-', '_', $theme);
    $params = array(
      'theme' => $theme,
    );
    $skins = skinr_skin_load_multiple(skinr_skin_get_sids($params));

    // Convert classes to arrays.
    $code = '';
    foreach ($skins as $skin) {
      unset($skin->sid);
      unset($skin->rdf_mapping);
      $code .= '$skins[] = '. var_export((array) $skin, TRUE) .";\n";
    }

    $lines = substr_count($code, "\n") + 1;

    $form['theme'] = array(
      '#type' => 'textfield',
      '#title' => t('Theme'),
      '#value' => $themes[$theme]->info['name'],
      '#disabled' => TRUE,
    );

    $form['skinr_settings'] = array(
      '#type' => 'textarea',
      '#title' => t('Skinr settings'),
      '#default_value' => $code,
      '#rows' => min($lines, 150),
    );
  }
  else {
    // Give the option for which theme to export.

    $options = array();
    ksort($themes);
    $current_theme = skinr_current_theme(TRUE);

    // Put default theme at the top.
    $options[$current_theme] = $themes[$current_theme]->info['name']. ' [' . t('default') . ']';

    foreach ($themes as $theme) {
      if (!empty($theme->info['hidden'])) {
        continue;
      }

      if ($theme->name == $current_theme) {
        // Do nothing.
      }
      elseif ($theme->status) {
        $options[$theme->name] = $theme->info['name'] . ' [' . t('enabled') . ']';
      }
      else {
        $options[$theme->name] = $theme->info['name'];
      }
    }

    $form['theme'] = array(
      '#type' => 'select',
      '#title' => t('Theme'),
      '#description' => t('Theme to export the skinr settings for.'),
      '#options' => $options,
      '#required' => TRUE,
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Export'),
    );
  }

  return $form;
}

/**
 * Form validation handler for skinr_ui_export_form().
 */
function skinr_ui_export_form_validate(&$form, &$form_state) {
  if (!empty($form_state['values']['theme']) && preg_match('/[^a-zA-Z0-9_]/', $form_state['values']['theme'])) {
    form_error($form['theme'], t('The theme name must be alphanumeric and can contain underscores only.'));
  }
}

/**
 * Form submission handler for skinr_ui_export_form().
 */
function skinr_ui_export_form_submit(&$form, &$form_state) {
  drupal_goto('admin/structure/skinr/export/' . str_replace('_', '-', $form_state['values']['theme']));
}

/**
 * Form builder for the Skinr settings import form.
 *
 * @ingroup forms
 */
function skinr_ui_import_form($form, &$form_state) {
  $form['skin_configurations'] = array(
    '#type' => 'textarea',
    '#title' => t('Skin configurations'),
    '#description' => t('Paste skin coonfigurations here.'),
    '#rows' => 16,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Import'),
  );

  return $form;
}

/**
 * Form validation handler for skinr_ui_import_form().
 */
function skinr_ui_import_form_validate(&$form, &$form_state) {
  if (empty($form_state['values']['skin_configurations'])) {
    // Error.
    form_error($form['skin_configurations'], t('These are not valid skin configurations.'));
    return;
  }

  $skins = '';
  ob_start();
  eval($form_state['values']['skin_configurations']);
  ob_end_clean();

  foreach ($skins as $key => $skin) {
    if (!is_array($skin)) {
      form_error($form['skin_configurations'], t('These are not valid skin configurations.'));
      break;
    }
    $skins[$key] = (object) $skin;
    if (!skinr_skin_validate($skins[$key])) {
      form_error($form['skin_configurations'], t('These are not valid skin configurations.'));
    }
  }

  $form_state['skins'] = &$skins;
}

/**
 * Form submission handler for skinr_ui_import_form().
 */
function skinr_ui_import_form_submit(&$form, &$form_state) {
  foreach ($form_state['skins'] as $skin) {
    // Find existing skin configuration and grab its sid.
    $params = array(
      'theme' => $skin->theme,
      'module' => $skin->module,
      'element' => $skin->element,
      'skin' => $skin->skin,
    );
    $sids = skinr_skin_get_sids($params);
    if (!empty($sids)) {
      $skin->sid = reset($sids);
    }

    // Save skin configuration.
    if (!skinr_skin_save($skin)) {
      drupal_set_message(t('Not all skin configurations could be saved!'), 'error', FALSE);
    }
  }

  drupal_set_message(t('The skin configurations have been saved.'));
  drupal_goto('admin/structure/skinr');
}

/**
 * Form builder for the skinr settings delete confirmation form.
 *
 * @param $theme
 *   The name of the theme to delete a setting for.
 * @param $module
 *   The module to delete a setting for.
 * @param $element
 *   The ID of the setting to delete.
 *
 * @ingroup forms
 */
function skinr_ui_delete_confirm($form, &$form_state, $skin) {
  $form['#skin'] = $skin;
  // Always provide skin configuration sid in the same form key as in the skin
  // configuration edit form.
  $form['sid'] = array(
    '#type' => 'value',
    '#value' => $skin->sid,
  );

  $themes = list_themes();
  return confirm_form($form,
    t('Are you sure you want to delete this skin configuration?'),
    isset($_GET['destination']) ? $_GET['destination'] : 'admin/structure/skinr',
    t('This action cannot be undone.<br />Theme: %theme<br />Module: %module<br />Element: %element<br />Skin: %skin', array('%theme' => $themes[$skin->theme]->info['name'], '%module' => $skin->module, '%element' => $skin->element, '%skin' => $skin->skin)),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Form submission handler for skinr_ui_delete_confirm().
 */
function skinr_ui_delete_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    skinr_skin_delete($form_state['values']['sid']);
    watchdog('content', 'Deleted a skin configuration.');
    drupal_set_message(t('A skin configuration has been deleted.'));
  }

  $form_state['redirect'] = 'admin/structure/skinr';
}
