// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../icons.html.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_page/settings_section.js';
import '../settings_page_styles.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';

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
      prefs: {
        type: Object,
        notify: true,
      },
      isManaged_: {
        type: Boolean,
        value: false,
      },

      userAgent: String,
      oldUserAgent: String,
      userAgentChangedAndValid: Boolean,
    };
  }

  userAgent: string;
  userAgentChangedAndValid: boolean;
  oldUserAgent: String;

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      const userAgent = this.getPref('settings.chameleon.user_agent').value;

      this.userAgent = userAgent;
      this.oldUserAgent = userAgent;
      this.userAgentChangedAndValid = false;
    });
  }

  private userAgentIsValid() {
    return true;
  }

  private onUserAgentInput_() {
    if (this.oldUserAgent !== this.userAgent) {
      if (this.userAgentIsValid()) {
        this.userAgentChangedAndValid = true;
      }
      return;
    }
    this.userAgentChangedAndValid = false;
  }

  private onSaveUserAgentButtonClick_() {
    this.setPrefValue('settings.chameleon.user_agent', this.userAgent);
    this.oldUserAgent = this.userAgent;
    this.userAgentChangedAndValid = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-profile-page': ProfilePageElement;
  }
}

customElements.define(ProfilePageElement.is, ProfilePageElement);
