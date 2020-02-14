Global Payments Gateway
----------------------
A Payment gateway for Global Payments (formerly called Realex Payments).  To use
this module you will need to have an account set up with Global Payments and
have received a shared key as well as a merchant ID. Global Payments have the
requirement that you initially use the account in test mode to make sure that
the process works.

Test cards available at
https://developer.realexpayments.com/#!/resources/test-card-numbers

Installation
------------
 - To make sure the correct dependencies are pulled in through composer, you have to add the following
 to the `repositories` section of your root `composer.json`:
```
{
    "type": "package",
    "package": {
        "name": "globalpayments/rxp-js",
        "version": "1.3.1",
        "type": "drupal-library",
        "dist": {
            "url": "https://github.com/globalpayments/rxp-js/archive/v1.3.1.zip",
            "type": "zip"
        },
        "require": {
            "composer/installers": "~1.0"
        }
    }
},
{
    "type": "vcs",
    "url": "https://github.com/annertech/php-sdk"
}
```
 - Download and enable the module
 - Go to the payment methods settings page at admin/commerce/config/payment-gateways
 - Select "Add Payment Gateway" and add the Global Payments details
