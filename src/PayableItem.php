<?php

namespace Drupal\commerce_realex;

/**
 * Represents a Payable Item.
 */
class PayableItem implements PayableItemInterface {

  /**
   * The private temp-store.
   *
   * @var \Drupal\user\PrivateTempStore
   */
  protected $paymentTempStore;

  /**
   * The shared payment temp-store.
   *
   * @var \Drupal\Core\TempStore\SharedTempStore
   */
  protected $paymentSharedTempStore;

  /**
   * Array of key:value.
   *
   * @var array
   *
   * @todo Use separate class members instead of an array?
   *
   * @todo ROAD MAP
   *  - abstract this to a PayableItemInterface?
   */
  protected $values;

  /**
   * Constructor.
   */
  public function __construct() {
    // @todo Use dependency injection to obtain these services.
    $this->paymentTempStore = \Drupal::service('user.private_tempstore')
      ->get('commerce_realex');
    /** @var \Drupal\user\SharedTempStore */
    $this->paymentSharedTempStore = \Drupal::service('tempstore.shared')
      ->get('commerce_realex');
  }

  /**
   * {@inheritDoc}
   */
  public function saveTempStore($uuid = NULL) {
    $uuid_service = \Drupal::service('uuid');
    if (!$uuid) {
      $uuid = $uuid_service->generate();
    }

    // Build temporary storage object from essential data.
    $storage_data = [
      'class' => __CLASS__,
      'values' => $this->values,
    ];

    // Save it to private temp store under the UUID "payment object" key.
    $this->paymentTempStore->set($uuid, $storage_data);

    return $uuid;
  }

  /**
   * {@inheritDoc}
   */
  public static function createFromPaymentTempStore(string $uuid) {
    $payable = new static();

    $temp_item = $payable->paymentTempStore->get($uuid);

    // @todo validate that the $temp_item['class'] matches __CLASS__
    foreach ($temp_item['values'] as $key => $value) {
      $payable->setValue($key, $value);
    }

    return $payable;
  }

  /**
   * Save a PayableItem in SharedtempStore.
   *
   * We don't need to save anything about the Payment itself permanently.
   * Items using this class can use the Realex Order ID as payment ID.
   *
   * @param string $uuid
   *   The UUID of the PayableItem.
   *
   * @return string
   *   The key the object was stored under in the Shared temp Store;
   *
   * @throws \Drupal\Core\TempStore\TempStoreException
   *
   * @todo ROAD MAP
   *  - abstract some of this to a PermitInterface?
   * @todo corresponding method to create from temp store, by UUID.
   */
  public function saveSharedTempStore($uuid = NULL) {
    $uuid_service = \Drupal::service('uuid');
    if (!$uuid) {
      $uuid = $uuid_service->generate();
    }

    // Build temporary storage object from essential data.
    $storage_data = [
      'class' => __CLASS__,
      'values' => $this->values,
    ];

    // Save it to private temp store under the UUID "payment object" key.
    $this->paymentSharedTempStore->set($uuid, $storage_data);

    return $uuid;
  }

  /**
   * Retrieve a payable from Shared Temp Store.
   *
   * @param string $uuid
   *   A UUID previously used to store data in the $paymentSharedTempStore.
   *
   * @return \Drupal\commerce_realex\PayableItem
   *   The retrieved payable item.
   *
   * @todo ROAD-MAP
   *   - formalize this in some interface, e.g. PayableItem
   */
  public static function createFromPaymentSharedTempStore(string $uuid) {
    $payable = new static();

    $temp_item = $payable->paymentSharedTempStore->get($uuid);

    // @todo validate that the $temp_item['class'] matches __CLASS__
    foreach ($temp_item['values'] as $key => $value) {
      $payable->setValue($key, $value);
    }

    return $payable;
  }

  /**
   * {@inheritDoc}
   */
  public function setValue($key, $value) {
    // @todo validation?
    $this->values[$key] = $value;
  }

  /**
   * {@inheritDoc}
   */
  public function getValue($key) {
    return $this->values[$key];
  }

  /**
   * {@inheritdoc}
   */
  public function getPayableAmount() {
    // @todo Could this be more robust by looking up the permit type (GUID)?
    if (isset($this->values['payable_amount'])) {
      return round((float) $this->values['payable_amount'], 2);
    }

    throw new \InvalidArgumentException('Amount not set.');
  }

  /**
   * {@inheritdoc}
   */
  public function getPayableCurrency() {
    if (isset($this->values['payable_currency'])) {
      return $this->values['payable_currency'];
    }

    throw new \InvalidArgumentException('Currency not set.');
  }

}
