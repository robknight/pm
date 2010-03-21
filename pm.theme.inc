<?php
// $Id$ 

function theme_pm_contract_list($form)
{
  $rows = array();
    
  foreach (element_children($form) as $key) {
  //dpm($key);
    // No need to print the field title every time.
    //unset($form[$key]['chtext']['#title'], $form[$key]['chvotes']['#title']);
    $title = $form[$key]['title']['#title'];
    unset($form[$key]['title']['#title']);
    // Build the table row.
    $row = array(
      'data' => array(
        array('data' => $title),
        array('data' => drupal_render($form[$key])),
      ),
    );

    // Add additional attributes to the row, such as a class for this row.
    if (isset($form[$key]['#attributes'])) {
      $row = array_merge($row, $form[$key]['#attributes']);
    }
    $rows[] = $row;
  }

  $output = theme('table', $headers, $rows);
 // $output .= drupal_render($form);
  return $output;
}

function theme_pm_contracts($contracts)
{
  $access = user_access('trade in markets');
  $output = '<ul class="pm-contracts">';
  foreach ($contracts as $c) {
    $output .= '<li>' . $c['title'] . ' ($' . $c['price'] . ')' .  ($access ? drupal_get_form('pm_trade_form', $c['cid']) : '') . '</li>';
  }
  $output .= '</ul>';

  return theme('box', t('Contracts'), $output);
}


function theme_pm_block_user($user)
{
  return t("Balance: !balance", array('!balance' => $user->balance));
}

function theme_pm_block_holdings($user, $market)
{
  $items = array();
  
  foreach ($market->pm_contracts as $c) {
    if ($p = $user->portfolio[$c['cid']]) {
      $title = $c['title'];
      $quantity = $p['quantity'];
      $items[] = $title . ': ' . $quantity;
    }
  }
  
  if (count($items)) {  
    return theme('item_list', $items);
  } else {
    return t("No contracts owned in this market");
  }
}