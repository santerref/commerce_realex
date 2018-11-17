<?php

namespace Drupal\commerce_realex\PluginForm;

use Drupal\commerce_payment\PluginForm\PaymentOffsiteForm;
use Drupal\commerce_order\Entity\Order;
use Drupal\Core\Form\FormStateInterface;
use Symfony\Component\HttpFoundation\Request;
use Drupal\Core\Url;
use Drupal\commerce_realex\PayableItem;

class RedirectCheckoutForm extends PaymentOffsiteForm {

  public function buildConfigurationForm(array $form, FormStateInterface $form_state) {
    $form = parent::buildConfigurationForm($form, $form_state);

    /** @var \Drupal\commerce_payment\Entity\PaymentInterface $payment */
    $payment = $this->entity;
		$order = $payment->getOrder();
		$profile = $order->getBillingProfile();
		$address =  $order->getBillingProfile()->get('address')->first();

    $plugin = $payment->getPaymentGateway()->getPlugin();

    $payable = new PayableItem();
    // Setup Custom Values on the Payable.
    $payable->setValue('payable_amount', $payment->getAmount()->getNumber());
    $payable->setValue('given_name', $address->getGivenName());
    $payable->setValue('family_name', $address->getFamilyName());
		$payable->setValue('commerce_order_id', $order->id());
    $payable->setValue('realex_config', $plugin->getConfiguration());

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
