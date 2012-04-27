<?php

function bartik1_preprocess_html(&$variables) {
  // Add a class to the body element
  $variables['classes_array'][] = 'my-class';
}

/**
 * Below is a sample function to add a variable to the views_view.tpl.php file.
function bartik1_preprocess_views_view(&$variables, $hook) {
  $variables['sample_variable'] = "<h1>Hello World</h1>";
}
*/

/**
 * Adds a link at the bottom of all views to a printer friendly version of that view. I could not find
 * a default way to add this from the views module.
 */
function overrides_preprocess_views_view__vacant_lots(&$variables, $hook) {
 $redirect_query_string = "";
 $redirect_url = "";
 
 if (isset($_SERVER['REDIRECT_QUERY_STRING'])) {
   $redirect_query_string = "?" . $_SERVER['REDIRECT_QUERY_STRING'];
 }
 
 if (isset($_SERVER['REDIRECT_URL'])) {
   $redirect_url = $_SERVER['REDIRECT_URL'];
 }
 
 $printer_friendly_url = "/print" . $redirect_url . $redirect_query_string;
 $printer_friendly_link = "<a href=\"" . $printer_friendly_url . "\">Printer-friendly version</a>";
 
 
 $variables['printer_friendly_link'] = $printer_friendly_link;
}