// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../relaunch_confirmation_dialog.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';

import {getTemplate} from './profile_page.html.js';

const ProfilePageElementBase =
    RelaunchMixin(WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class ProfilePageElement extends ProfilePageElementBase {
  static get is() {
    return 'settings-profile-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      userAgentSubstitution: {
        type: Object,
        value() {
          return {type: chrome.settingsPrivate.PrefType.BOOLEAN};
        },
      },
      userAgent: String,
      oldUserAgent: String,
      userAgentChangedAndValid: Boolean,

      productSubSubstitution: {
        type: Object,
        value() {
          return {type: chrome.settingsPrivate.PrefType.BOOLEAN};
        },
      },
      productSub: String,
      oldProductSub: String,
      productSubChangedAndValid: Boolean,

      platformSubstitution: {
        type: Object,
        value() {
          return {type: chrome.settingsPrivate.PrefType.BOOLEAN};
        },
      },
      platform: String,
      oldPlatform: String,
      platformChangedAndValid: Boolean,

      vendorSubstitution: {
        type: Object,
        value() {
          return {type: chrome.settingsPrivate.PrefType.BOOLEAN};
        },
      },
      vendor: String,
      oldVendor: String,
      vendorChangedAndValid: Boolean,
    };
  }

  declare private userAgentSubstitution: chrome.settingsPrivate.PrefObject<boolean>;
  declare userAgent: string;
  declare userAgentChangedAndValid: boolean;
  declare oldUserAgent: String;

  declare private productSubSubstitution: chrome.settingsPrivate.PrefObject<boolean>;
  declare productSub: string;
  declare productSubChangedAndValid: boolean;
  declare oldProductSub: String;

  declare private platformSubstitution: chrome.settingsPrivate.PrefObject<boolean>;
  declare platform: string;
  declare platformChangedAndValid: boolean;
  declare oldPlatform: String;

  declare private vendorSubstitution: chrome.settingsPrivate.PrefObject<boolean>;
  declare vendor: string;
  declare vendorChangedAndValid: boolean;
  declare oldVendor: String;

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      // ---- userAgent
      const userAgent = this.getPref('settings.chameleon.user_agent').value;
      const userAgentSubstitution = this.getPref('settings.chameleon.user_agent_substitution');

      this.userAgentSubstitution = userAgentSubstitution;
      this.userAgent = userAgent;
      this.oldUserAgent = userAgent;
      this.userAgentChangedAndValid = false;

      // ---- navigator.productSub
      const productSub = this.getPref('settings.chameleon.product_sub').value;
      const productSubSubstitution = this.getPref('settings.chameleon.product_sub_substitution');

      this.productSubSubstitution = productSubSubstitution;
      this.productSub = productSub;
      this.oldProductSub = productSub;
      this.productSubChangedAndValid = false;

      // ---- navigator.platform
      const platform = this.getPref('settings.chameleon.platform').value;
      const platformSubstitution = this.getPref('settings.chameleon.platform_substitution');

      this.platformSubstitution = platformSubstitution;
      this.platform = platform;
      this.oldPlatform = platform;
      this.platformChangedAndValid = false;

      // ---- navigator.vendor
      const vendor = this.getPref('settings.chameleon.vendor').value;
      const vendorSubstitution = this.getPref('settings.chameleon.vendor_substitution');

      this.vendorSubstitution = vendorSubstitution;
      this.vendor = vendor;
      this.oldVendor = vendor;
      this.vendorChangedAndValid = false;
    });
  }

  // ---- userAgent
  private onUserAgentSubstitutionToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.setPrefValue('settings.chameleon.user_agent_substitution', !!target.checked);
  }

  private onUserAgentInput_() {
    if (this.oldUserAgent !== this.userAgent) {
      this.userAgentChangedAndValid = true;
      return;
    }
    this.userAgentChangedAndValid = false;
  }

  private onSaveUserAgentButtonClick_() {
    this.setPrefValue('settings.chameleon.user_agent', this.userAgent);
    this.oldUserAgent = this.userAgent;
    this.userAgentChangedAndValid = false;
  }

  // ---- navigator.productSub
  private onProductSubSubstitutionToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.setPrefValue('settings.chameleon.product_sub_substitution', !!target.checked);
  }

  private onProductSubInput_() {
    if (this.oldProductSub !== this.productSub) {
      this.productSubChangedAndValid = true;
      return;
    }
    this.productSubChangedAndValid = false;
  }

  private onSaveProductSubButtonClick_() {
    this.setPrefValue('settings.chameleon.product_sub', this.productSub);
    this.oldProductSub = this.productSub;
    this.productSubChangedAndValid = false;
  }

  // ---- navigator.platform
  private onPlatformSubstitutionToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.setPrefValue('settings.chameleon.platform_substitution', !!target.checked);
  }

  private onPlatformInput_() {
    if (this.oldPlatform !== this.platform) {
      this.platformChangedAndValid = true;
      return;
    }
    this.platformChangedAndValid = false;
  }

  private onSavePlatformButtonClick_() {
    this.setPrefValue('settings.chameleon.platform', this.platform);
    this.oldPlatform = this.platform;
    this.platformChangedAndValid = false;
  }

  // ---- navigator.vendor
  private onVendorSubstitutionToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.setPrefValue('settings.chameleon.vendor_substitution', !!target.checked);
  }

  private onVendorInput_() {
    if (this.oldVendor !== this.vendor) {
      this.vendorChangedAndValid = true;
      return;
    }
    this.vendorChangedAndValid = false;
  }

  private onSaveVendorButtonClick_() {
    this.setPrefValue('settings.chameleon.vendor', this.vendor);
    this.oldVendor = this.vendor;
    this.vendorChangedAndValid = false;
  }

}

declare global {
  interface HTMLElementTagNameMap {
    'settings-profile-page': ProfilePageElement;
  }
}

customElements.define(ProfilePageElement.is, ProfilePageElement);
