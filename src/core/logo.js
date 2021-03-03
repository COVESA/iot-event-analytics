/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('./util/logger');

const IOTEA_LOGO = `                (@@@#
                                    #@@@&
                                    #@@@&
                                    #@@@&  @@@@,
                                    #@@@&  @@@@(
                                    #@@@&  @@@@(
                              *@&.  #@@@&  @@@@(
                             ,@@@@  #@@@&  @@@@(   &@/
                             ,@@@@  #@@@&  @@@@(  @@@@*
                             ,@@@@  #@@@&  @@@@(  @@@@*
                             .@@@@  #@@@&  #@@@   @@@@*
                               ..   #@@@&         @@@@,
                                    #@@@&         (@@@
                                     @@@

               /@@@@@@@@@@@/            ,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@&
               #@@@@@@@@                    #@@@@@@@@@@@@@@@@@@@@       &@@#
               #@@@@@@     ,@@@@@@@@@@@@#     #@@@@@@@@@@@@@@@@@@      #@@@
               #@@@@%    (@@@@@@@@@@@@@@@@&     @@@@@@@@@@@@@@@@*    *@@@%
               #@@@@    %@@@@@@@@@@@@@@@@@@@    (@@@@@@@@@@@@@@@  ,@@@@%
               #@@@@    @@@@@@@@@@@@@@@@@@@@/    @@@@@@@@@@@@@@@@@@@%
               #@@@@    @@@@@@@@@@@@@@@@@@@@,   *@@@@@@@@@@@@@@&.
               #@@@@    .@@@@@@@@@@@@@@@@@@/    @@@@@@@@@@@@@%
               #@@@@      /@@@@@@@@@@@@@@&     @@@@@@@@@@@@@*
               #@@@@         .&@@@@@@@*      &@@@@@@@@@@@@@
               #@@@@                      *@@@@@@@@@@@@@(
               *@@@/                  *@@@@@@@@@@@@@/*


               --------------------------- IoTEA ---------------------------
               -------------------- IoT Event Analytics --------------------`;

class Logo {
    constructor() {}
}

Logo.logger = new Logger('Logo');

Logo.print = () => {
    const matches = IOTEA_LOGO.match(/^(.*)$/gm)

    Logo.__printSpacer(2);

    for (let i = 1; i < matches.length; i++) {
      Logo.logger.always(matches[i]);
    }

    Logo.__printSpacer(2);
};

Logo.__printSpacer = (len) => {
  for (let i = 0;i < len; i++) {
    Logo.logger.always('');
  }
};

module.exports = Logo;