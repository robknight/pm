<?php


function _pm_add_contract()
{
  include_once 'modules/node/node.pages.inc';
  $form_state = array('storage' => NULL, 'submitted' => FALSE);
  $form_build_id = $_POST['form_build_id'];
  // Get the form from the cache.
  $form = form_get_cache($form_build_id, $form_state);
  $args = $form['#parameters'];
  $form_id = array_shift($args);
  // We will run some of the submit handlers so we need to disable redirecting.
  $form['#redirect'] = FALSE;
  // We need to process the form, prepare for that by setting a few internals
  // variables.
  $form['#post'] = $_POST;
  $form['#programmed'] = FALSE;
  $form_state['post'] = $_POST;
  //$output = dpr($form_state, true);  
  // Build, validate and if possible, submit the form.
  drupal_process_form($form_id, $form, $form_state);

  // This call recreates the form relying solely on the form_state that the
  // drupal_process_form set up.
  $form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);
  
  // Render the new output.
  $contract_form = $form['pm_contracts_wrapper']['pm_contracts'];
  unset($contract_form['#prefix'], $contract_form['#suffix']); // Prevent duplicate wrappers.
  $output .= theme('status_messages') . drupal_render($contract_form);

  drupal_json(array('status' => TRUE, 'data' => $output));
}

function pm_add_contract_submit($form, &$form_state) {
  // Set the form to rebuild and run submit handlers.
  node_form_submit_build_node($form, $form_state);
  if ($c = $form_state['values']['pm_new_contract']) {
    $tmp = array('cid' => 0, 'title' => $c);
    $form_state['new_contract'] = $tmp;
  }
}


/**
 * How the buying process works:
 *
 * The user specifies a contract to purchase, and two other values.  The first
 * is the maximum quantity to buy, and the second is the maximum price to pay
 * for any unit in the transaction.
 *
 * The market maker system will attempt to satisfy the user's command to the
 * greatest extent possible.  In the best-case scenario, this means purchasing
 * exactly $quantity contracts at a price lower than $max_price.  However, if
 * this is not possible then the quantity to purchase may be reduced.
 *
 * There are two reasons why the quantity may be reduced: firstly, because the
 * user has insufficient funds to purchase the full amount requested, and
 * secondly because the act of purchasing this quantity of contracts would
 * increase the price beyond the maximum price specified.  In either case, the
 * system works out how many contracts are purchasable within those parameters
 * and purchases only that number.
 */

function _pm_buy($cid, $quantity, $max_price)
{
  if (!is_numeric($cid) || !is_numeric($quantity)) {
    drupal_not_found();
  }
  
  $max_price /= 100;
  
  _pm_lock_tables(array('pm_contract' => 'write', 'pm_account' => 'write', 'pm_portfolio' => 'write',
                        'pm_transaction' => 'write'));
 // db_unlock_tables();
 // $account = _pm_load_current_account();
//  $market = _pm_get_node_from_cid($cid);
  $account = _pm_load_current_account();
  $contracts = _pm_contract_set_from_cid($cid);
//  $contracts = $market->pm_contracts;

  if ($contracts[$cid]['price'] > $max_price) {
    db_unlock_tables();
    return _pm_fail(PM_PRICE_TOO_LOW);
  }
  
  $quantity = _pm_quantity_to_buy($contracts, $cid, $quantity, $max_price, $account['balance']);
  $cost = _pm_calculate_cost($contracts, $cid, $quantity);
  
  $contracts[$cid]['quantity'] += $quantity;
  
  _pm_update_prices($contracts);
  
  $price = $contracts[$cid]['price'];
  
  foreach ($contracts as $c) {
    _pm_save_contract($c['nid'], $c);
  }
  
  $account['balance'] -= $cost;
  
  $account['portfolio'][$cid]['quantity'] += $quantity;
  _pm_save_account($account);
  _pm_save_transaction('buy', $cid, $account['uid'], $quantity, $cost, $contracts[$cid]['price']);
  
  db_unlock_tables();
  
  drupal_set_message(t('Bought !qty contracts for a total cost of !cost',
                       array('!qty' => $quantity, '!cost' => $cost)));
  //drupal_set_message(var_export(compact('quantity', 'cost', 'account', 'price', 'contracts'), true));
  
  drupal_goto('node/' . $contracts[$cid]['nid']);
  //return var_export(compact('quantity', 'cost', 'account', 'price', 'contracts'), true); //$price;
}



function _pm_sell($cid, $quantity, $min_price)
{
  if (!is_numeric($cid) || !is_numeric($quantity)) {
    drupal_not_found();
  }
  
  $min_price /= 100;
  
  _pm_lock_tables(array('pm_contract' => 'write', 'pm_account' => 'write', 'pm_portfolio' => 'write',
                        'pm_transaction' => 'write'));

  $account = _pm_load_current_account();
  $contracts = _pm_contract_set_from_cid($cid);
  
  $nid = $contracts[$cid]['nid'];

  if ($contracts[$cid]['price'] < $min_price) {
  //  var_dump($contracts[$cid]['price']);
    db_unlock_tables();
    return _pm_fail(PM_PRICE_TOO_HIGH, "node/{$nid}");
  }
  
  if (!($portfolio = $account['portfolio'][$cid]) || $portfolio['quantity'] == 0) {
    db_unlock_tables();
    return _pm_fail(PM_INSUFFICIENT_PORTFOLIO, "node/{$nid}");
  }
  
  $quantity = 0 - _pm_quantity_to_sell($contracts, $cid, $quantity, $min_price, $portfolio);
  $cost = _pm_calculate_cost($contracts, $cid, $quantity);
  
  $contracts[$cid]['quantity'] += $quantity;
  
  _pm_update_prices($contracts);
  
  $price = $contracts[$cid]['price'];
  
  foreach ($contracts as $c) {
    _pm_save_contract($c['nid'], $c);
  }
  
  $account['portfolio'][$cid]['quantity'] += $quantity;
  $account['balance'] -= $cost;

  _pm_save_account($account);
  _pm_save_transaction('sell', $cid, $account['uid'], -$quantity, $cost, $contracts[$cid]['price']);
  
  db_unlock_tables();

  drupal_set_message(t('Sold !qty contracts for a total revenue of !rev',
                       array('!qty' => -$quantity, '!rev' => -$cost)));  
  
  drupal_goto('node/' . $nid);
  //return var_export(compact('quantity', 'cost', 'account', 'price', 'contracts'), true); //$price;
}