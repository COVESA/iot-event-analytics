<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Talent 2 Cloud CLI

Converts a given JavaScript Talent implementation into the respective form, for deployment as a Cloud function.
Currently only AWS Lambda function is supported.
The conversion process, does not convert your business logic. It only create a scaffold, where your business-logic can be implemented. (See: ./templates/aws.mustache in this folder)

## Usage

- Implement a JavaScript Talent as usual (subclassing Talent or Worker is supported at the moment)
- Use _t2c.js_ to convert the talent into Copy-Paste cloud form<br>
  `node t2c.js -f <some talent>.js -a "mqtt://localhost:1884:string" -p aws -o "<some talent>-aws.js"`<br>
  The parameters are explained here ```node t2c.js --help```

## AWS

- __Replace all \<TalentID\> placeholders by the id of the talent you want to deploy__
- Goto _https://aws.amazon.com_ and either Login or Signup
- Create a virtual (Talent-) device<br>
  You may only need one virtual device for all functions (It's just about getting and endpoint where to send messages to)
  - Click on _Iot Core_ in the _Services List_
  - Go to _Verwalten_ \> _Richtlinien_ \> _Create_<br>
    The new "Richtlinie" will be connected to your certificate to protect communication from the IoT Event Analytics platform with your virtual device
    - Click on extended and fill in the following
      - Name: _IoTEventAnalyticsBridge_
      - Aktion: _iot:*_
      - Ressourcen-ARN: _*_
      - Effekt: _Erlauben_
  - Go to _Verwalten_ \> _Typen_ \> _Erstellen_<br>
    This is basically a level of hierarchy to takeover settings, which are set on group level
    - Name it _IoTEventAnalyticsTalents_
  - Go to _Verwalten_ \> _Objekte_ \> _Erstellen_ \> _Einzelnes Object erstellen_<br>
    Now we create the new virtual device
    - Name it TalentBridge
    - Choose the type you created above (here: IoTEventAnalyticsTalents)
    - You dont need an object group
    - Create a certificate, which secures the communication between IoT Event Analytics and the virtual device
      - Create a one-click certificate or reuse an existing one
      - __!!!For one click certificates only: Download all 3 files and activate the certificate!!!__
      - Click on "Eine Richtlinie anfügen" and select the _IoTEventAnalyticsBridge_ "Richtlinie"
    - Select your newly created object and click on "Interagieren"
      - Note down the HTTPS API Endpoint. You will need that later
  - Up to this point, we have a device and protected / configured the access
- Create a Lambda function
  - Go to _Services_ \> Lambda \> Create function \> "Ohne Vorgabe erstellen"
  - Pick a name, which resembles your talent (e.g. my-new-cloud-talent) and pick Node.js 12.x as runtime
  - Create a new role for basic Lambda permissions (select the radio button). The name of the rule will be something like "my-new-cloud-talent-role-...."
  - Click on _Funktion erstellen_
  - The newly create role for your function needs permissions to publish messages to IoT core (read is automatically granted by basic Lambda permissions)
    - Open the "Berechtigungen"-Tab in your function overview and click on the newly create role like "my-new-cloud-talent-role-...." to open the IAM page
    - Click on "Richtlinie anfügen" and search for _iot-core-publish-policy_ to give the functions rights to publish messages
    - Add this "Richtlinie" to the role
- Configure the Lambda function
  - Click on _Auslöser hinzufügen_
    - Pick AWS IoT as source and User-defined IoT-Rule
    - Create a new rule for talent-discovery and name it _talentdiscoveryrule_
    - As a "Regel-Abfrageanweisung" enter ```SELECT * FROM configManager/talents/discover```
  - Create a second "Auslöser"
    - Click on _Auslöser hinzufügen_
    - Do the same as for the first but change the name to _\<TalentID\>events_ "Regel-Abfrageanweisung" to `SELECT * FROM talent/<TalentID>/events`
- Now it's time to use the t2c converter to get your existing local NodeJS talent implementation into Cloud-Form
  - Once converted you have to fill in the endpoint noted down during the object creation in the created file<br>
    It looks like this _a3r4wmwpe4yfzp-ats.iot.eu-central-1.amazonaws.com_<br>

    ```code
    const iotdata = new AWS.IotData({
      endpoint: '<INSERT ENDPOINT HERE>',
      apiVersion: '2015-05-28'
    });
    ```

  - Copy the whole contents of the file and paste it into the index.js of your Lamda function
- Provide certificates to the local Mosquitto MQTT Broker
  - The preconfigured Mosquitto MQTT Broker needs the certificates you created during the AWS process to authenticate at the MQTT endpoint
  - Copy all your certificates and keys you downloaded (despite the public key) during the object creation process at AWS into the following folder _\<iot event analytics project folder\>/docker/mosquitto/certs_
    - Download AmazonRootCA from [https://www.amazontrust.com/repository/AmazonRootCA1.cer](https://www.amazontrust.com/repository/AmazonRootCA1.cer)
    - Now you need to rename these files (and overwrite the existing ones)
      - _AmazonRootCA1.cer_ \>\> _ca.pem_
      - _\<\>-certificate.pem.crt.txt_ \>\> _client.crt_
      - _\<\>-private.pem.key_ \>\> _private.key_
- Update the _config.json_ configuration for mosquitto
  - set _platformId_ to an empty string
  - set platformIdSuffix to _TalentBridge_ (The virtual device name you chose before)
  - You might set _loglevel_ to _all_ if you want to see debug output of the local message broker
- Build and start the platform by following the guide given on the main project page _"Run a talent as AWS Lambda function"_
