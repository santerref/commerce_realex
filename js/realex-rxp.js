/**
 * @file
 * Javascript for the commerce_realex module.
 */

(function ($, Drupal, drupalSettings) {

  'use strict';

  Drupal.behaviors.realexPaymentRealexPaymentForm = {
    attach: function (context) {
      // get the HPP JSON from the server-side SDK
      $(document).ready(function () {
        $.getJSON(drupalSettings.realexPaymentForm.requestUrl, function (jsonFromServerSdk) {
          RealexHpp.init(drupalSettings.realexPaymentForm.payButtonId, drupalSettings.realexPaymentForm.responseUrl, jsonFromServerSdk);
          RealexHpp.setHppUrl(drupalSettings.realexPaymentForm.hppUrl);
        });
      });
    }
  };

})(jQuery, Drupal, drupalSettings);
