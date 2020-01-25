<?php

namespace Drupal\commerce_realex\PluginForm;

use Drupal\commerce_payment\PluginForm\PaymentOffsiteForm;
use Drupal\commerce_realex\PayableItem;
use Drupal\Core\Form\FormStateInterface;
use Drupal\Core\Url;

/**
 * Realex Redirect checkout form.
 */
class RedirectCheckoutForm extends PaymentOffsiteForm {

  /**
   * Build the configuration form.
   */
  public function buildConfigurationForm(array $form, FormStateInterface $form_state) {
    $form = parent::buildConfigurationForm($form, $form_state);

    /** @var \Drupal\commerce_payment\Entity\PaymentInterface $payment */
    $payment = $this->entity;
    $order = $payment->getOrder();
    $address = $order->getBillingProfile()->get('address')->first();
    $entity_type_manager = \Drupal::service('entity_type.manager');
    $currency_code = $payment->getAmount()->getCurrencyCode();
    /** @var \Drupal\commerce_price\Entity\CurrencyInterface $currency */
    $currency = $entity_type_manager->getStorage('commerce_currency')
      ->load($currency_code);
    $currency_symbol = $currency->getSymbol();

    $plugin = $payment->getPaymentGateway()->getPlugin();

    $payable = new PayableItem();
    // Setup Custom Values on the Payable.
    $payable->setValue('payable_amount', $payment->getAmount()->getNumber());
    $payable->setValue('payable_currency', $currency_code);
    $payable->setValue('payable_currency_symbol', $currency_symbol);
    $payable->setValue('given_name', $address->getGivenName());
    $payable->setValue('family_name', $address->getFamilyName());
    $payable->setValue('commerce_order_id', $order->id());
    $payable->setValue('realex_config', $plugin->getConfiguration());
    $payable->setValue('payable_uid', $order->getCustomerId());

    $data = [];
    $temp_store_key = $payable->saveTempStore();

    // Redirect to Payment Form.
    $url = Url::fromRoute('commerce_realex.payment_form', ['payable_item_id' => $temp_store_key]);

    return $this->buildRedirectForm(
      $form,
      $form_state,
      $url->toString(),
      $data,
      PaymentOffsiteForm::REDIRECT_POST
    );
  }

}
