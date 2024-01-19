// Copyright 2016 The Chromium Authors
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

// import {loadTimeData} from '../i18n_setup.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';

import {getTemplate} from './hashnet_page.html.js';
// import {PromoteUpdaterStatus} from './about_page_browser_proxy.js';


const HashNetPageElementBase =
    RelaunchMixin(WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class HashNetPageElement extends HashNetPageElementBase {
  static get is() {
    return 'settings-hashnet-page';
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
      value: String,
      privateKey: String,
    };
  }

  value: string;
  privateKey: string;

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      const agentsList = this.getPref('settings.hashnet.agents_list').value;
      const privateKey = this.getPref('settings.hashnet.private_key').value;
      console.log("SETTINGS!!!", agentsList, privateKey);
      this.value = agentsList;
      this.privateKey = privateKey;
    });
  }

  private onSaveButtonClick_() {
    this.setPrefValue('settings.hashnet.agents_list', this.value);
  }

  private onSavePrivateKeyButtonClick_() {
    this.setPrefValue('settings.hashnet.private_key', this.privateKey);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-hashnet-page': HashNetPageElement;
  }
}

customElements.define(HashNetPageElement.is, HashNetPageElement);
