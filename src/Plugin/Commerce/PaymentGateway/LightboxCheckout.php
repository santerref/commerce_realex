<?php

namespace Drupal\commerce_realex\Plugin\Commerce\PaymentGateway;

use Drupal\commerce_payment\Plugin\Commerce\PaymentGateway\OffsitePaymentGatewayBase;
use Drupal\Core\Form\FormStateInterface;
use Drupal\commerce_order\Entity\OrderInterface;
use Drupal\commerce_payment\Exception\PaymentGatewayException;
use Symfony\Component\HttpFoundation\Request;

/**
 * Provides the Global Payments Lightbox Checkout payment gateway.
 *
 * @CommercePaymentGateway(
 *   id = "realex_lightbox_checkout",
 *   label = @Translation("Global Payments (Lightbox with Iframe)"),
 *   display_label = @Translation("Global Payments"),
 *    forms = {
 *     "offsite-payment" = "Drupal\commerce_realex\PluginForm\RedirectCheckoutForm",
 *   },
 *   payment_method_types = {"credit_card"},
 *   credit_card_types = {
 *     "mastercard", "visa",
 *   },
 * )
 */
class LightboxCheckout extends OffsitePaymentGatewayBase {

	public function defaultConfiguration() {
	return [
			'realex_server_url' => '',
			'realex_account' => '',
			'realex_shared_secret' => '',
			'realex_merchant_id' => '',
		] + parent::defaultConfiguration();
	}

  public function buildConfigurationForm(array $form, FormStateInterface $form_state) {
    $form = parent::buildConfigurationForm($form, $form_state);

    $form['realex_server_url'] = [
      '#type' => 'textfield',
      '#title' => $this->t('Global Payments Server URL'),
      '#description' => $this->t('Global Payments Server URL for payment requests.'),
      '#default_value' => $this->configuration['realex_server_url'],
      '#required' => TRUE,
    ];

    $form['realex_merchant_id'] = [
      '#type' => 'textfield',
      '#title' => $this->t('Global Payments Merchant ID'),
      '#description' => $this->t('This is the Merchant ID provided by Global Payments.'),
      '#default_value' => $this->configuration['realex_merchant_id'],
      '#required' => TRUE,
    ];

    $form['realex_account'] = [
      '#type' => 'textfield',
      '#title' => $this->t('Global Payments Account'),
      '#description' => $this->t('This is the Global Payments Account.'),
      '#default_value' => $this->configuration['realex_account'],
      '#required' => TRUE,
    ];

    $form['realex_shared_secret'] = [
      '#type' => 'textfield',
      '#title' => $this->t('Global Payments Shared Secret'),
      '#description' => $this->t('This is the shared secret provided by Global Payments.'),
      '#default_value' => $this->configuration['realex_shared_secret'],
      '#required' => TRUE,
    ];

    return $form;
  }

	public function submitConfigurationForm(array &$form, FormStateInterface $form_state) {
    parent::submitConfigurationForm($form, $form_state);
    if (!$form_state->getErrors()) {
      $values = $form_state->getValue($form['#parents']);
      $this->configuration['realex_server_url'] = $values['realex_server_url'];
      $this->configuration['realex_merchant_id'] = $values['realex_merchant_id'];
      $this->configuration['realex_account'] = $values['realex_account'];
      $this->configuration['realex_shared_secret'] = $values['realex_shared_secret'];
    }
  }

	public function onReturn(OrderInterface $order, Request $request) {
		// Get Payment ID from Request.
		$payable_item_id = $request->query->get('payable_item_id');
    try {
      $this->payableItemId =  $payable_item_id;
      $this->paymentTempStore = \Drupal::service('user.private_tempstore')->get('commerce_realex');
      $payable_item_class = $this->paymentTempStore->get($payable_item_id)['class'];
      $this->payableItem = $payable_item_class::createFromPaymentTempStore($payable_item_id);
    }
    catch (\Exception $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
		// Only Process completed Payments.
    if ($this->payableItem->getValue('payment_complete') !== TRUE) {
        throw new PaymentGatewayException('Payment failed!');
    }

    $payment_storage = $this->entityTypeManager->getStorage('commerce_payment');
    $payment = $payment_storage->create([
      'state' => 'completed',
      'amount' => $order->getTotalPrice(),
      'payment_gateway' => $this->entityId,
      'order_id' => $order->id(),
      'remote_id' => $this->payableItem->getValue('authCode'),
      'remote_state' => $request->request->get('remote_state'),
    ]);

    $payment->save();
	}
}
