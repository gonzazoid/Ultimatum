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

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// import {loadTimeData} from '../i18n_setup.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
// import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
// import {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';

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
      agentsList: String,
      oldAgentsList: String,
      agentsListChanged: String,

      privateKey: String,
      oldPrivateKey: String,
      privateKeyChangedAndValid: Boolean,
    };
  }

  declare private agentsList: string;
  declare private agentsListChanged: boolean;
  declare private oldAgentsList: string;

  declare private privateKey: string;
  declare private privateKeyChangedAndValid: boolean;
  declare private oldPrivateKey: String;

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      const agentsList = this.getPref('settings.hashnet.agents_list').value;
      const privateKey = this.getPref('settings.hashnet.private_key').value;

      this.agentsList = agentsList;
      this.oldAgentsList = agentsList;
      this.agentsListChanged = false;

      this.privateKey = privateKey;
      this.oldPrivateKey = privateKey;
      this.privateKeyChangedAndValid = false;
    });
  }

  private privateKeyIsValid() {
    const supportedSignFunctions = [
      "secp256r1",
    ];
    const keyLengths = {
      secp256r1: 64,
    } as { [key: string]: number };

    const supportedHashFunctions = [
      "sha1",
      "sha256",
      "sha512",
    ];

    if (this.privateKey === "") return true;
    const tokens = this.privateKey.split(":");
    if (tokens.length !== 2) return false;
    const [signFormula, keyValue] = tokens;
    const formulaTokens = signFormula.split(".");
    if (formulaTokens.length !==2) return false;
    const [signFunction, hashFunction] = formulaTokens;

    if (!supportedSignFunctions.includes(signFunction)) return false;
    if (!supportedHashFunctions.includes(hashFunction)) return false;

    if (keyValue.length !== keyLengths[signFunction]) return false;
    const isHex = /^[0-9a-f]+$/g;
    if (!isHex.test(tokens[1])) return false;

    return true;
  }

  private onPrivateKeyInput_() {
    if (this.oldPrivateKey !== this.privateKey) {
      if (this.privateKeyIsValid()) {
        this.privateKeyChangedAndValid = true;
      }
      return;
    }
    this.privateKeyChangedAndValid = false;
  }

  private onSavePrivateKeyButtonClick_() {
    this.setPrefValue('settings.hashnet.private_key', this.privateKey);
    this.oldPrivateKey = this.privateKey;
    this.privateKeyChangedAndValid = false;
  }

  private onAgentsListInput_() {
    if (this.agentsList !== this.oldAgentsList) {
      this.agentsListChanged = true;
    } else {
      this.agentsListChanged = false;
    }
  }

  private onSaveAgentsListButtonClick_() {
    this.setPrefValue('settings.hashnet.agents_list', this.agentsList);
    this.oldAgentsList = this.agentsList;
    this.agentsListChanged = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-hashnet-page': HashNetPageElement;
  }
}

customElements.define(HashNetPageElement.is, HashNetPageElement);
