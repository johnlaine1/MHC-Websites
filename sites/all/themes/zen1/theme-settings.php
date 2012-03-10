<?php
/**
 * Implements hook_form_system_theme_settings_alter().
 *
 * @param $form
 *   Nested array of form elements that comprise the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 */
function zen1_form_system_theme_settings_alter(&$form, &$form_state)  {

  // Create the form using Forms API: http://api.drupal.org/api/7

  $form['zen1_settings'] = array(
    '#type'          => 'fieldset',
    '#title'         => t('My Sample Settings'),
  );  
  
  $form['zen1_settings'] ['main_menu_styles'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Black Gradient Menu'),
    '#default_value' => theme_get_setting('main_menu_styles'),
    '#description'   => t('This option will change the appearance of the Main Menu'),
  );
  
/*  $form['zen1_settings']['main_menu_styles'] = array(
    '#type'          => 'select',
    '#title'         => t('Select the style of the Main Menu'),
    '#default_value' => theme_get_setting('main_menu_styles'),
    '#options'       => array(
                          'bubplastic' => t('Black Bubble Type'),
                          'main_menu_tabs' => t('Tabs'),                          
                        ),
  );*/

  // Remove some of the base theme's settings.
  unset($form['themedev']['zen_layout']); // We don't need to select the layout stylesheet.

  // We are editing the $form in place, so we don't need to return anything.
}
