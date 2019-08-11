<?php

/**
 * @file
 * Callback methods for Global Payments Redirect integration.
 */

define('GATEWAY_URL', 'https://hpp.realexpayments.com/pay');
define('GATEWAY_URL_TEST', 'https://hpp.sandbox.realexpayments.com/pay');

/**
 * Payment method callback: settings form.
 */
function commerce_realex_redirect_commerce_payment_method_settings_form($settings = NULL) {
  $form = [];

  $form['merchant_id'] = [
    '#type' => 'textfield',
    '#title' => t('Global Payments Merchant ID'),
    '#default_value' => isset($settings['merchant_id']) ? $settings['merchant_id'] : '',
    '#size' => 16,
    '#required' => TRUE,
  ];

  $form['account_id'] = [
    '#type' => 'textfield',
    '#title' => t('Global Payments Account ID'),
    '#default_value' => isset($settings['account_id']) ? $settings['account_id'] : '',
    '#size' => 16,
    '#required' => TRUE,
  ];

  $form['shared_secret'] = [
    '#type' => 'textfield',
    '#title' => t('Shared secret'),
    '#default_value' => isset($settings['shared_secret']) ? $settings['shared_secret'] : '',
    '#size' => 16,
    '#required' => TRUE,
  ];

  $form['gateway_url'] = [
    '#type' => 'textfield',
    '#title' => t('Gateway URL'),
    '#default_value' => isset($settings['gateway_url']) ? $settings['gateway_url'] : GATEWAY_URL,
    '#required' => TRUE,
  ];

  $form['gateway_url_test'] = [
    '#type' => 'textfield',
    '#title' => t('Gateway Test URL'),
    '#default_value' => isset($settings['gateway_test_url']) ? $settings['gateway_test_url'] : GATEWAY_URL_TEST,
    '#required' => TRUE,
  ];

  $form['test_mode'] = [
    '#type' => 'checkbox',
    '#title' => t('Enable test mode'),
    '#description' => t('If enabled, the transactions go to a sandbox Global Payments server.'),
    '#default_value' => isset($settings['test_mode']) ? $settings['test_mode'] : 0,
  ];

  $form['redirect_referring_url'] = [
    '#type' => 'textfield',
    '#title' => t('Referring URL'),
    '#description' => t('The absolute url to the page on your site from which
    the Global Payments gateway will be referred by. You will need to give this url to Global Payments when they set up your account.'),
    '#default_value' => isset($settings['redirect_referring_url']) ? $settings['redirect_referring_url'] : url('checkout/', ['absolute' => TRUE]) . '(.*)',
    '#required' => TRUE,
    '#disabled' => TRUE,
  ];

  $form['auto_settle'] = [
    '#type' => 'select',
    '#title' => t('Payment action'),
    '#options' => [1 => t('Authorize and Capture'), 0 => t('Authorize only')],
    '#default_value' => isset($settings['auto_settle'])   $settings['auto_settle'] : 1,
    '#required' => TRUE,
  ];

  $form['varref'] = [
    '#type' => 'textfield',
    '#title' => t('Variable reference'),
    '#description' => t('Any reference you also would like to assign to the customer. This can allow checking, using realscore, of previous transactions by this customer.'),
    '#default_value' => isset($settings['varref']) ? $settings['varref'] : '',
    '#size' => 50,
    '#required' => FALSE,
  ];

  return $form;
}

/**
 * Implements hook_commerce_payment_method_submit_form().
 *
 * Add a message and CSS to the submission form.
 */
function commerce_realex_redirect_commerce_payment_method_submit_form($payment_method, $pane_values, $checkout_pane, $order) {
  $form['commmerce_realex_redirect_markup'] = [
    '#markup' => '<span class="commerce-realex-redirect-information">' . t('You will be redirected to the Global Payments Hosted Payment Page to complete your order.') . '</span>',
  ];

  return $form;
}

/**
 * Payment method callback: redirect form, a wrapper around the module's general
 * use function for building a realex form.
 */
function commerce_realex_redirect_redirect_form($form, &$form_state, $order, $payment_method) {
  // Return an error if the enabling action's settings haven't been configured.
  if (empty($payment_method['settings']['merchant_id'])) {
    drupal_set_message(t('Global Payments is not configured for use. No Global Payments merchant ID has been specified.'), 'error');
    return [];
  }

  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);

  $request = [];
  $request['currency'] = $order_wrapper->commerce_order_total->currency_code->value();
  $request['amount'] = $order_wrapper->commerce_order_total->amount->value();

  // Get products purchased.
  $products = [];
  $comments = [];
  foreach ($order_wrapper->commerce_line_items as $delta => $line_item_wrapper) {
    $line_item = $line_item_wrapper->value();
    // If the line item is a product line item...
    if (in_array($line_item->type, commerce_product_line_item_types())) {
      $product = $line_item_wrapper->commerce_product->value();
      $products[] = $product->product_id;
      $comments[] = sprintf("%dx %s", $line_item->quantity, check_plain($product->title));
    }
  }

  $request['products'] = implode(',', $products);
  $request['comments'][0] = t('Products purchased:') . ' ' . implode(', ', $comments);

  $request['timestamp'] = strftime('%Y%m%d%H%M%S');
  mt_srand((double) microtime() * 1000000);

  $request['order_number'] = $order->order_number;
  $request['orderid'] = $order->order_id . '-' . $request['timestamp'];
  $request['merchantid'] = check_plain($payment_method['settings']['merchant_id']);
  $request['account'] = check_plain($payment_method['settings']['account_id']);
  $request['secret'] = check_plain($payment_method['settings']['shared_secret']);
  $request['auto_settle'] = $payment_method['settings']['auto_settle'];
  $request['varref'] = $payment_method['settings']['varref'];

  // Allow other modules to alter the variable reference - used to tag orders.
  drupal_alter('commerce_realex_varref', $request['varref'], $order);
  $request['varref'] = check_plain($request['varref']);

  $request['sha1hash'] = _commerce_realex_build_request_sha1hash($request, 'redirect-auth');

  // Build the data array that will be translated into hidden form values.
  $data = [
    'MERCHANT_RESPONSE_URL' => url('commerce-realex/redirect/complete', ['absolute' => TRUE]),
    'MERCHANT_ID' => $request['merchantid'],
    'ORDER_ID' => $request['orderid'],
    'ACCOUNT' => $request['account'],
    'AMOUNT' => $request['amount'],
    'CURRENCY' => $request['currency'],
    'TIMESTAMP' => $request['timestamp'],
    'SHA1HASH' => $request['sha1hash'],
    'AUTO_SETTLE_FLAG' => $request['auto_settle'],
    'VAR_REF' => $request['varref'],
    'COMMERCE_ORDER_ID' => $order->order_id,
    'COMMERCE_ORDER_NUMBER' => $order->order_number,
    'RETURN_TSS' => 1,
  ];

  if (!empty($request['products'])) {
    $data['PROD_ID'] = $request['products'];
  }
  if (!empty($request['comments']) && is_array($request['comments'])) {
    $data['COMMENT1'] = $request['comments'][0];
    if (strlen($data['COMMENT1']) > 255) {
      $data['COMMENT1'] = substr($data['COMMENT1'], 0, 252) . '...';
    }
  }
//  $data['COMMENT2'] = _commerce_realex_get_module_versions();

  $additional_hpp_data = [
    "HPP_VERSION"        => 2,
    "HPP_CUSTOMER_EMAIL" => isset($order->mail) ? $order->mail : '',
    "COMMENT1"           => 'drupalcommerce7',
  ];

  // Load the customer billing profile for the order.
  if (!empty($order->commerce_customer_billing)) {
    $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
    $profile = $order_wrapper->commerce_customer_billing->value();
    $profile_wrapper = entity_metadata_wrapper('commerce_customer_profile', $profile);
    $address = $profile_wrapper->commerce_customer_address->value();

    $data['CUST_NUM'] = $profile->profile_id;
    $street_one = isset($address['thoroughfare']) ? check_plain($address['thoroughfare']) : '';
    $postcode = isset($address['postal_code']) ? check_plain($address['postal_code']) : '';
    $zip_numbers = preg_replace('/[^0-9]/', '', $postcode);
    $street_numbers = preg_replace('/[^0-9]/', '', $street_one);
    $zip_postal_code = $zip_numbers . '|' . $street_numbers;
    $zip_postal_code = ($zip_postal_code == '|') ? '' : $zip_postal_code;
    $country = isset($address['country']) ? check_plain($address['country']) : '';

    $data['BILLING_CO'] = $country;
    $data['BILLING_CODE'] = $zip_postal_code;

    $hpp_billing_state = '';

    if (!empty($country) && in_array($address['country'], ['US', 'CA'])) {
      if (isset($address['administrative_area']) && !empty($address['administrative_area'])) {
        $hpp_billing_state = $address['administrative_area'];
      }
    }

    $hpp_billing_fields    = [
      "HPP_BILLING_STREET1"    => $street_one,
      "HPP_BILLING_STREET2"    => '',
      "HPP_BILLING_STREET3"    => '',
      "HPP_BILLING_CITY"       => isset($address['locality']) ? $address['locality'] : '',
      "HPP_BILLING_STATE"      => $hpp_billing_state,
      "HPP_BILLING_POSTALCODE" => $zip_numbers,
      "HPP_BILLING_COUNTRY"    => !empty($country) ? commerce_realex_get_country_numeric_code($country) : '',
    ];
    $additional_hpp_data[] = $hpp_billing_fields;
  }
  else {
    // Don't think you can have orders without billing details, but let's stay
    // on the safe side.
    $hpp_billing_fields = [
      "HPP_BILLING_STREET1"    => '',
      "HPP_BILLING_STREET2"    => '',
      "HPP_BILLING_STREET3"    => '',
      "HPP_BILLING_CITY"       => '',
      "HPP_BILLING_STATE"      => '',
      "HPP_BILLING_POSTALCODE" => '',
      "HPP_BILLING_COUNTRY"    => '',
    ];
  }

  // Load the customer shipping profile for the order.
  if (!empty($order->commerce_customer_shipping)) {
    $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
    $profile = $order_wrapper->commerce_customer_shipping->value();
    $profile_wrapper = entity_metadata_wrapper('commerce_customer_profile', $profile);
    $address = $profile_wrapper->commerce_customer_address->value();

    $data['CUST_NUM'] = !empty($data['CUST_NUM']) ? $data['CUST_NUM'] : $profile->profile_id;
    $street_one = isset($address['thoroughfare']) ? check_plain($address['thoroughfare']) : '';
    $postcode = isset($address['postal_code']) ? check_plain($address['postal_code']) : '';
    $zip_numbers = preg_replace('/[^0-9]/', '', $postcode);
    $street_numbers = preg_replace('/[^0-9]/', '', $street_one);
    $zip_postal_code = substr($zip_numbers, 0, 5) . '|' . substr($street_numbers, 0, 5);
    $zip_postal_code = ($zip_postal_code == '|') ? '' : $zip_postal_code;
    $country = isset($address['country']) ? check_plain($address['country']) : '';

    $data['SHIPPING_CO'] = $country;
    $data['SHIPPING_CODE'] = $zip_postal_code;

    $hpp_shipping_state = '';

    if (!empty($country) && in_array($address['country'], ['US', 'CA'])) {
      if (isset($address['administrative_area']) && !empty($address['administrative_area'])) {
        $hpp_shipping_state = $address['administrative_area'];
      }
    }

    $hpp_shipping_fields = [
      "HPP_SHIPPING_STREET1"    => $street_one,
      "HPP_SHIPPING_STREET2"    => '',
      "HPP_SHIPPING_STREET3"    => '',
      "HPP_SHIPPING_CITY"       => isset($address['locality']) ? $address['locality'] : '',
      "HPP_SHIPPING_STATE"      => $hpp_shipping_state,
      "HPP_SHIPPING_POSTALCODE" => $zip_numbers,
      "HPP_SHIPPING_COUNTRY"    => !empty($country) ? commerce_realex_get_country_numeric_code($country) : '',
    ];
  }
  else {
    $hpp_shipping_fields = [
      "HPP_SHIPPING_STREET1"    => '',
      "HPP_SHIPPING_STREET2"    => '',
      "HPP_SHIPPING_STREET3"    => '',
      "HPP_SHIPPING_CITY"       => '',
      "HPP_SHIPPING_STATE"      => '',
      "HPP_SHIPPING_POSTALCODE" => '',
      "HPP_SHIPPING_COUNTRY"    => '',
    ];
  }

  // Order and type matter.
  if (array_values($hpp_billing_fields) === array_values($hpp_shipping_fields)) {
    $additional_hpp_data["HPP_ADDRESS_MATCH_INDICATOR"] = "TRUE";
    $additional_hpp_data[]                              = $hpp_shipping_fields;
  }
  else {
    $additional_hpp_data["HPP_ADDRESS_MATCH_INDICATOR"] = "FALSE";
    $additional_hpp_data[]                              = $hpp_shipping_fields;
  }

  foreach ($additional_hpp_data as $additional_hpp_prop => $additional_hpp_value) {
    if (is_array($additional_hpp_value)) {
      foreach ($additional_hpp_value as $additional_hpp_prop_child => $additional_hpp_value_child) {
        $data[$additional_hpp_prop_child] = $additional_hpp_value_child;
      }
    }
    else {
      $data[$additional_hpp_prop] = $additional_hpp_value;
    }
  }

  // Allow other modules to modify the data - useful if they want to change the
  // response url or add additional data to the request (to be returned in the
  // response).
  drupal_alter('commerce_realex_redirect_data', $data, $order);

  // Live url.
  $form['#action'] = !empty($payment_method['settings']['gateway_url']) ? $payment_method['settings']['gateway_url'] : GATEWAY_URL;
  // Sandbox.
  if (!empty($payment_method['settings']['test_mode'])) {
    $form['#action'] = !empty($payment_method['settings']['gateway_url']) ? $payment_method['settings']['gateway_url_test'] : GATEWAY_URL_TEST;
  }

  foreach ($data as $name => $value) {
    if (!empty($value)) {
      $form[$name] = ['#type' => 'hidden', '#value' => $value];
    }
  }

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Proceed to Global Payments'),
  ];

  return $form;
}

/**
 * Returns an array of numeric country codes, indexed by alpha-2 code.
 *
 * @return array
 *   Array of numeric country codes.
 */
function commerce_realex_get_country_numeric_codes() {
  return [
    'AF' => '004',
    'AX' => '248',
    'AL' => '008',
    'DZ' => '012',
    'AS' => '016',
    'AD' => '020',
    'AO' => '024',
    'AI' => '660',
    'AQ' => '010',
    'AG' => '028',
    'AR' => '032',
    'AM' => '051',
    'AW' => '533',
    'AU' => '036',
    'AT' => '040',
    'AZ' => '031',
    'BS' => '044',
    'BH' => '048',
    'BD' => '050',
    'BB' => '052',
    'BY' => '112',
    'BE' => '056',
    'BZ' => '084',
    'BJ' => '204',
    'BM' => '060',
    'BT' => '064',
    'BO' => '068',
    'BQ' => '535',
    'BA' => '070',
    'BW' => '072',
    'BV' => '074',
    'BR' => '076',
    'IO' => '086',
    'BN' => '096',
    'BG' => '100',
    'BF' => '854',
    'BI' => '108',
    'CV' => '132',
    'KH' => '116',
    'CM' => '120',
    'CA' => '124',
    'KY' => '136',
    'CF' => '140',
    'TD' => '148',
    'CL' => '152',
    'CN' => '156',
    'CX' => '162',
    'CC' => '166',
    'CO' => '170',
    'KM' => '174',
    'CG' => '178',
    'CD' => '180',
    'CK' => '184',
    'CR' => '188',
    'CI' => '384',
    'HR' => '191',
    'CU' => '192',
    'CW' => '531',
    'CY' => '196',
    'CZ' => '203',
    'DK' => '208',
    'DJ' => '262',
    'DM' => '212',
    'DO' => '214',
    'EC' => '218',
    'EG' => '818',
    'SV' => '222',
    'GQ' => '226',
    'ER' => '232',
    'EE' => '233',
    'ET' => '231',
    'SZ' => '748',
    'FK' => '238',
    'FO' => '234',
    'FJ' => '242',
    'FI' => '246',
    'FR' => '250',
    'GF' => '254',
    'PF' => '258',
    'TF' => '260',
    'GA' => '266',
    'GM' => '270',
    'GE' => '268',
    'DE' => '276',
    'GH' => '288',
    'GI' => '292',
    'GR' => '300',
    'GL' => '304',
    'GD' => '308',
    'GP' => '312',
    'GU' => '316',
    'GT' => '320',
    'GG' => '831',
    'GN' => '324',
    'GW' => '624',
    'GY' => '328',
    'HT' => '332',
    'HM' => '334',
    'VA' => '336',
    'HN' => '340',
    'HK' => '344',
    'HU' => '348',
    'IS' => '352',
    'IN' => '356',
    'ID' => '360',
    'IR' => '364',
    'IQ' => '368',
    'IE' => '372',
    'IM' => '833',
    'IL' => '376',
    'IT' => '380',
    'JM' => '388',
    'JP' => '392',
    'JE' => '832',
    'JO' => '400',
    'KZ' => '398',
    'KE' => '404',
    'KI' => '296',
    'KP' => '408',
    'KR' => '410',
    'KW' => '414',
    'KG' => '417',
    'LA' => '418',
    'LV' => '428',
    'LB' => '422',
    'LS' => '426',
    'LR' => '430',
    'LY' => '434',
    'LI' => '438',
    'LT' => '440',
    'LU' => '442',
    'MO' => '446',
    'MK' => '807',
    'MG' => '450',
    'MW' => '454',
    'MY' => '458',
    'MV' => '462',
    'ML' => '466',
    'MT' => '470',
    'MH' => '584',
    'MQ' => '474',
    'MR' => '478',
    'MU' => '480',
    'YT' => '175',
    'MX' => '484',
    'FM' => '583',
    'MD' => '498',
    'MC' => '492',
    'MN' => '496',
    'ME' => '499',
    'MS' => '500',
    'MA' => '504',
    'MZ' => '508',
    'MM' => '104',
    'NA' => '516',
    'NR' => '520',
    'NP' => '524',
    'NL' => '528',
    'NC' => '540',
    'NZ' => '554',
    'NI' => '558',
    'NE' => '562',
    'NG' => '566',
    'NU' => '570',
    'NF' => '574',
    'MP' => '580',
    'NO' => '578',
    'OM' => '512',
    'PK' => '586',
    'PW' => '585',
    'PS' => '275',
    'PA' => '591',
    'PG' => '598',
    'PY' => '600',
    'PE' => '604',
    'PH' => '608',
    'PN' => '612',
    'PL' => '616',
    'PT' => '620',
    'PR' => '630',
    'QA' => '634',
    'RE' => '638',
    'RO' => '642',
    'RU' => '643',
    'RW' => '646',
    'BL' => '652',
    'SH' => '654',
    'KN' => '659',
    'LC' => '662',
    'MF' => '663',
    'PM' => '666',
    'VC' => '670',
    'WS' => '882',
    'SM' => '674',
    'ST' => '678',
    'SA' => '682',
    'SN' => '686',
    'RS' => '688',
    'SC' => '690',
    'SL' => '694',
    'SG' => '702',
    'SX' => '534',
    'SK' => '703',
    'SI' => '705',
    'SB' => '090',
    'SO' => '706',
    'ZA' => '710',
    'GS' => '239',
    'SS' => '728',
    'ES' => '724',
    'LK' => '144',
    'SD' => '729',
    'SR' => '740',
    'SJ' => '744',
    'SE' => '752',
    'CH' => '756',
    'SY' => '760',
    'TW' => '158',
    'TJ' => '762',
    'TZ' => '834',
    'TH' => '764',
    'TL' => '626',
    'TG' => '768',
    'TK' => '772',
    'TO' => '776',
    'TT' => '780',
    'TN' => '788',
    'TR' => '792',
    'TM' => '795',
    'TC' => '796',
    'TV' => '798',
    'UG' => '800',
    'UA' => '804',
    'AE' => '784',
    'GB' => '826',
    'US' => '840',
    'UM' => '581',
    'UY' => '858',
    'UZ' => '860',
    'VU' => '548',
    'VE' => '862',
    'VN' => '704',
    'VG' => '092',
    'VI' => '850',
    'WF' => '876',
    'EH' => '732',
    'YE' => '887',
    'ZM' => '894',
    'ZW' => '716',
  ];
}

/**
 * Returns an array of numeric country phone codes, indexed by alpha-2 country
 * code.
 *
 * @return array
 *   Array of numeric country phone codes.
 */
function commerce_realex_get_country_phone_codes() {
  return [
    'AD' => '376',
    'AE' => '971',
    'AF' => '93',
    'AG' => '1268',
    'AI' => '1264',
    'AL' => '355',
    'AM' => '374',
    'AN' => '599',
    'AO' => '244',
    'AQ' => '672',
    'AR' => '54',
    'AS' => '1684',
    'AT' => '43',
    'AU' => '61',
    'AW' => '297',
    'AZ' => '994',
    'BA' => '387',
    'BB' => '1246',
    'BD' => '880',
    'BE' => '32',
    'BF' => '226',
    'BG' => '359',
    'BH' => '973',
    'BI' => '257',
    'BJ' => '229',
    'BL' => '590',
    'BM' => '1441',
    'BN' => '673',
    'BO' => '591',
    'BR' => '55',
    'BS' => '1242',
    'BT' => '975',
    'BW' => '267',
    'BY' => '375',
    'BZ' => '501',
    'CA' => '1',
    'CC' => '61',
    'CD' => '243',
    'CF' => '236',
    'CG' => '242',
    'CH' => '41',
    'CI' => '225',
    'CK' => '682',
    'CL' => '56',
    'CM' => '237',
    'CN' => '86',
    'CO' => '57',
    'CR' => '506',
    'CU' => '53',
    'CV' => '238',
    'CX' => '61',
    'CY' => '357',
    'CZ' => '420',
    'DE' => '49',
    'DJ' => '253',
    'DK' => '45',
    'DM' => '1767',
    'DO' => '1809',
    'DZ' => '213',
    'EC' => '593',
    'EE' => '372',
    'EG' => '20',
    'ER' => '291',
    'ES' => '34',
    'ET' => '251',
    'FI' => '358',
    'FJ' => '679',
    'FK' => '500',
    'FM' => '691',
    'FO' => '298',
    'FR' => '33',
    'GA' => '241',
    'GB' => '44',
    'GD' => '1473',
    'GE' => '995',
    'GH' => '233',
    'GI' => '350',
    'GL' => '299',
    'GM' => '220',
    'GN' => '224',
    'GQ' => '240',
    'GR' => '30',
    'GT' => '502',
    'GU' => '1671',
    'GW' => '245',
    'GY' => '592',
    'HK' => '852',
    'HN' => '504',
    'HR' => '385',
    'HT' => '509',
    'HU' => '36',
    'ID' => '62',
    'IE' => '353',
    'IL' => '972',
    'IM' => '44',
    'IN' => '91',
    'IQ' => '964',
    'IR' => '98',
    'IS' => '354',
    'IT' => '39',
    'JM' => '1876',
    'JO' => '962',
    'JP' => '81',
    'KE' => '254',
    'KG' => '996',
    'KH' => '855',
    'KI' => '686',
    'KM' => '269',
    'KN' => '1869',
    'KP' => '850',
    'KR' => '82',
    'KW' => '965',
    'KY' => '1345',
    'KZ' => '7',
    'LA' => '856',
    'LB' => '961',
    'LC' => '1758',
    'LI' => '423',
    'LK' => '94',
    'LR' => '231',
    'LS' => '266',
    'LT' => '370',
    'LU' => '352',
    'LV' => '371',
    'LY' => '218',
    'MA' => '212',
    'MC' => '377',
    'MD' => '373',
    'ME' => '382',
    'MF' => '1599',
    'MG' => '261',
    'MH' => '692',
    'MK' => '389',
    'ML' => '223',
    'MM' => '95',
    'MN' => '976',
    'MO' => '853',
    'MP' => '1670',
    'MR' => '222',
    'MS' => '1664',
    'MT' => '356',
    'MU' => '230',
    'MV' => '960',
    'MW' => '265',
    'MX' => '52',
    'MY' => '60',
    'MZ' => '258',
    'NA' => '264',
    'NC' => '687',
    'NE' => '227',
    'NG' => '234',
    'NI' => '505',
    'NL' => '31',
    'NO' => '47',
    'NP' => '977',
    'NR' => '674',
    'NU' => '683',
    'NZ' => '64',
    'OM' => '968',
    'PA' => '507',
    'PE' => '51',
    'PF' => '689',
    'PG' => '675',
    'PH' => '63',
    'PK' => '92',
    'PL' => '48',
    'PM' => '508',
    'PN' => '870',
    'PR' => '1',
    'PT' => '351',
    'PW' => '680',
    'PY' => '595',
    'QA' => '974',
    'RO' => '40',
    'RS' => '381',
    'RU' => '7',
    'RW' => '250',
    'SA' => '966',
    'SB' => '677',
    'SC' => '248',
    'SD' => '249',
    'SE' => '46',
    'SG' => '65',
    'SH' => '290',
    'SI' => '386',
    'SK' => '421',
    'SL' => '232',
    'SM' => '378',
    'SN' => '221',
    'SO' => '252',
    'SR' => '597',
    'ST' => '239',
    'SV' => '503',
    'SY' => '963',
    'SZ' => '268',
    'TC' => '1649',
    'TD' => '235',
    'TG' => '228',
    'TH' => '66',
    'TJ' => '992',
    'TK' => '690',
    'TL' => '670',
    'TM' => '993',
    'TN' => '216',
    'TO' => '676',
    'TR' => '90',
    'TT' => '1868',
    'TV' => '688',
    'TW' => '886',
    'TZ' => '255',
    'UA' => '380',
    'UG' => '256',
    'US' => '1',
    'UY' => '598',
    'UZ' => '998',
    'VA' => '39',
    'VC' => '1784',
    'VE' => '58',
    'VG' => '1284',
    'VI' => '1340',
    'VN' => '84',
    'VU' => '678',
    'WF' => '681',
    'WS' => '685',
    'XK' => '381',
    'YE' => '967',
    'YT' => '262',
    'ZA' => '27',
    'ZM' => '260',
    'ZW' => '263',
  ];
}

/**
 * Given an alpha-2 country code, returns the country numeric code.
 *
 * @param string $alpha2
 *   Alpha-2 country code.
 *
 * @return mixed|string
 *   Numeric country code or empty string.
 */
function commerce_realex_get_country_numeric_code($alpha2) {
  $countries = commerce_realex_get_country_numeric_codes();

  return isset($countries[$alpha2]) ? $countries[$alpha2] : '';
}

/**
 * Returns an array of all possible language codes.
 */
function commerce_realex_redirect_languages() {
  return drupal_map_assoc(['AU', 'DE', 'FR', 'IT', 'GB', 'ES', 'US']);
}

/**
 * Returns an array of all possible currency codes.
 */
function commerce_realex_redirect_currencies() {
  return drupal_map_assoc([
    'AUD',
    'BRL',
    'CAD',
    'CHF',
    'CZK',
    'DKK',
    'EUR',
    'GBP',
    'HKD',
    'HUF',
    'ILS',
    'JPY',
    'MXN',
    'MYR',
    'NOK',
    'NZD',
    'PHP',
    'PLN',
    'SEK',
    'SGD',
    'THB',
    'TWD',
    'USD',
  ]);
}

/**
 * Checkout completion function.
 */
function commerce_realex_redirect_checkout_complete() {
  $request = [];

  // Pull out the response data.
  $response = [];
  foreach ($_POST as $key => $value) {
    // Modify keys to lower case.
    $key = strtolower($key);
    $response[$key] = $value;
  }

  if (!empty($response['commerce_order_id'])) {
    $order = commerce_order_load($response['commerce_order_id']);

    $instance_id = $order->data['payment_method'];
    $payment_method = commerce_payment_method_instance_load($instance_id);

    $success = _commerce_realex_redirect_verify_response($response, $order, $payment_method);

    // Save the transaction.
    $log = isset($request['log_message']) ? $request['log_message'] : '';
    $transaction = commerce_realex_redirect_transaction_save($payment_method, $order, $response, $log);

    if ($success) {
      // Mark the order as complete.
      commerce_checkout_complete($order);

      $url = url('checkout/' . $order->order_id . '/complete', ['absolute' => TRUE]);
      return _commerce_realex_redirect_back($url);
    }
  }

  // Invalid response or failure.
  $message = t('We were unable to process your credit card payment; reason provided by bank: %message. If the problem persists, contact us to complete your order.', ['%message' => $response['message']]);
  $order = commerce_order_status_update($order, 'checkout_checkout', FALSE, TRUE, t('Global Payments Redirect:') . ' ' . $message);
  drupal_set_message($message, 'warning');
  watchdog('commerce_realex', 'Payment failed: %message', ['%message' => $response['message']], WATCHDOG_WARNING);
  $uri = commerce_checkout_order_uri($order);
  $url = url($uri, ['absolute' => TRUE, 'query' => ['realex' => serialize($message)]]);
  return _commerce_realex_redirect_back($url);
}

/**
 * Redirect back to given url on the website.
 */
function _commerce_realex_redirect_back($url) {
  $output = '<style type="text/css">body {display: none;}</style>';
  $output .= '<script type="text/javascript"> window.location = "' . $url . '";</script>';
  print $output;
  return '';
}

/**
 * Process the Global Payments response.
 *
 * @param array $response
 *   Parsed response as an array.
 * @param object $order
 *   Commerce order object.
 * @param array $payment_method
 *   Payment method array.
 */
function _commerce_realex_redirect_verify_response(&$response, $order, $payment_method) {
  if (!isset($response['result'])) {
    watchdog('commerce_realex', 'No results code, something has gone wrong with the transaction.', [], WATCHDOG_ERROR);
    drupal_goto('checkout/' . $order->order_id . '/payment');
  }

  list($order_id, $timestamp) = explode('-', $response['order_id']);

  $merchantid = $payment_method['settings']['merchant_id'];
  $secret = $payment_method['settings']['shared_secret'];
  $orderid = $response['order_id'];
  $resultcode = $response['result'];
  $message = $response['message'];
  if ($response['result'] != '00') {
    return FALSE;
  }
  $pasref = isset($response['pasref']) ? $response['pasref'] : '';
  $authcode = isset($response['authcode']) ? $response['authcode'] : '';

  // This digital signature should correspond to the one Global Payments returns
  // and can therefore be used to verify the message Global Payments sends back.
  $tmp = "$timestamp.$merchantid.$orderid.$resultcode.$message.$pasref.$authcode";
  $sha1hash = sha1($tmp);
  $tmp = "$sha1hash.$secret";
  $sha1hash = sha1($tmp);

  $success = FALSE;

  // Check to see if hashes match or not.
  if ($sha1hash != $response['sha1hash']) {
    watchdog('commerce_realex', 'Hash match failed for order @order_id.', ['@order_id' => $response['order_id']]);
    drupal_set_message(t("Hashes don't match - response not authenticated!"), 'error');
    $response['user_message'] = 'Payment error, please try again later';
  }
  else {
    $success = TRUE;
  }

  unset($response['sha1hash']);

  return $success;
}

/**
 * Save the transaction and update the order.
 *
 * @param array $payment_method
 *   Payment method array.
 * @param object $order
 *   Commerce order object.
 * @param array $response
 *   Response data.
 * @param string $log
 *   Optional extra log message.
 */
function commerce_realex_redirect_transaction_save($payment_method, $order, $response, $log = '') {
  $transaction = commerce_payment_transaction_new('commerce_realex_redirect', $order->order_id);
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  $commerce_order_total = $order_wrapper->commerce_order_total->value();
  $transaction->amount = $commerce_order_total['amount'];
  $transaction->currency_code = $commerce_order_total['currency_code'];
  $transaction->instance_id = $payment_method['instance_id'];
  $transaction->remote_id = $response['commerce_order_id'];

  // Set the payment status.
  switch ($response['result']) {
    case '00':
      $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
      break;

    default:
      $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
  }

  // Log the response variables.
  $logged_vars = ['result', 'message', 'order_id', 'timestamp', 'form_build_id', 'form_token'];
  $transaction->message = 'Status @resultcode, @message.<br />Order ID: @orderid,<br />Timestamp: @timestamp<br />';
  $transaction->message_variables = [
    '@resultcode' => isset($response['result']) ? $response['result'] : '',
    '@message' => isset($response['message']) ? $response['message'] : '',
    '@orderid' => isset($response['order_id']) ? $response['order_id'] : '',
    '@timestamp' => isset($response['timestamp']) ? $response['timestamp'] : '',
  ];
  if (!empty($response['authcode'])) {
    $transaction->message .= 'Auth code: @authcode<br />';
    $transaction->message_variables['@authcode'] = $response['authcode'];
    $logged_vars[] = 'authcode';
  }
  if (!empty($response['pasref'])) {
    $transaction->message .= 'Pasref: @pasref<br />';
    $transaction->message_variables['@pasref'] = $response['pasref'];
    $logged_vars[] = 'pasref';
  }
  if (!empty($response['avsaddressresult'])) {
    $transaction->message .= 'Address check: @address<br />';
    $transaction->message_variables['@address'] = $response['avsaddressresult'];
    $logged_vars[] = 'avsaddressresult';
  }
  if (!empty($response['avspostcoderesult'])) {
    $transaction->message .= 'Postcode check: @postcode<br />';
    $transaction->message_variables['@postcode'] = $response['avspostcoderesult'];
    $logged_vars[] = 'avspostcoderesult';
  }
  if (!empty($response['cvnresult'])) {
    $transaction->message .= 'CVN result: @cvn<br />';
    $transaction->message_variables['@cvn'] = $response['cvnresult'];
    $logged_vars[] = 'cvnresult';
  }
  if (!empty($response['batchid'])) {
    $transaction->message .= 'Batch Id: @batchid<br />';
    $transaction->message_variables['@batchid'] = $response['batchid'];
    $logged_vars[] = 'batchid';
  }
  if (!empty($response['tss'])) {
    $transaction->message .= 'Transaction suitability score: @tss<br />';
    $transaction->message_variables['@tss'] = $response['tss'];
    $logged_vars[] = 'tss';
  }
  if (!empty($response['log'])) {
    $transaction->message .= '@log<br />';
    $transaction->message_variables['@log'] = $response['log'];
    $logged_vars[] = 'log';
  }
  if (!empty($response['eci'])) {
    $transaction->message .= 'ECI: @eci_scenario (@eci)<br />';
    $transaction->message_variables['@eci'] = $response['eci'];
    $transaction->message_variables['@eci_scenario'] = commerce_realex_redirect_3dsecure_eci_lookup($response['eci']);
    $logged_vars[] = 'eci';
    $logged_vars[] = 'eci_scenario';
  }
  if (!empty($response['xid'])) {
    $transaction->message .= 'XID: @xid<br />';
    $transaction->message_variables['@xid'] = $response['xid'];
    $logged_vars[] = 'xid';
  }
  if (!empty($response['caav'])) {
    $transaction->message .= 'CAAV: @caav<br />';
    $transaction->message_variables['@caav'] = $response['caav'];
    $logged_vars[] = 'caav';
  }

  // For remaining variables, just add them as key/value pairs to the log.
  $i = 0;
  foreach ($response as $key => $value) {
    if (!in_array($key, $logged_vars)) {
      $transaction->message .= "@key$i: @value$i<br />";
      $transaction->message_variables["@key$i"] = $key;
      $transaction->message_variables["@value$i"] = $value;
      $logged_vars[] = $key;
      $i++;
    }
  }

  $transaction->payload = $response;

  commerce_payment_transaction_save($transaction);

  return $transaction;
}

/**
 * Lookup ECI value returned and map it to scenario.
 */
function commerce_realex_redirect_3dsecure_eci_lookup($eci) {
  $scenario = NULL;
  switch ($eci) {
    case 5:
    case 2:
      $scenario = t('Full 3D Secure – cardholder Enrolled');
      break;

    case 6:
    case 1:
      $scenario = t('Merchant 3D Secure – cardholder not enrolled or attempt ACS server was used');
      break;

    case 7:
    case 0:
      $scenario = t('Non 3D Secure transaction');
      break;
  }
  return $scenario;
}

