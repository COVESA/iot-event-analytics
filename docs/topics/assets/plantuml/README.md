<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# PlantUML artifacts

This folder contains PlantUML files and the generated images which are referenced/used as diagrams in the documentation of this project. Changes of these diagrams have to be done here by changing the PlantUML files first and (re-)generating the image.

## How to visualize PlantUML files

PlantUML files in this folder can be visualized/generated using a PlantUML renderer.

### Visual Studio Code integration

Visual Studio provides a PlantUML integration using the extension 'PlantUML'.

Following configuration for this extension is recommended:
1. **Plantuml: Renderer**</br>
   Set Renderer here. For public information you could simply use the public plantuml renderer(https://www.plantuml.com/plantuml)
2. **Plantuml: Export Out Dir**</br>
   Set it to the current folder (./)
3. **Plantuml: File Extensions***</br>
   Disable this option to place the generated file beside the PlantUML file.
4. **Plantuml: Export Format**</br>
   Set it to svg

You can use _Alt+D_ on an open PlantUML file to render a preview in Visual Studio Code.

You can use _Ctrl+Shift+P_ and use commands provided for PlantUML to generate images files out of it (e.g. _PlantUML: Export current diagram_).

### Public plantuml-server

You can use the public PlantUML-server to render the content of these files. 

Open http://www.plantuml.com/plantuml/uml/ and paste in the content of the file you want to render and click on _submit_.