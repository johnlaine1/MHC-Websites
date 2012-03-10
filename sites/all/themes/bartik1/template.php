<?php
function bartik1_preprocess_html(&$variables) {
  // Add a class to the body element
  $variables['classes_array'][] = 'my-class';
}
