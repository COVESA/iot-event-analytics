<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Core library for Talent development

## Test your implementation using the source code and NOT the installed package

- Since we are using relative imports, your script must actually be treated as a module in the package tree or use the package tree directly residing next to the package root.
  Relative imports in Python depend on the `__name__` and the `__package__`, which are `__name__ = __main__` and `__package__ = None` if you invoke your script using `python run.py`. You will potentially receive this error `ImportError: attempted relative import with no known parent package`. You can get around this by using one of the following strategies:
  - __Open the _src_ directory in a terminal__
  - You can add a _run.py_ module next to the package root i.e. _src_ folder and import the module by specifying it's location in the package structure. This works, since _src/iotea_ is a package<br />

    ```python
    from iotea.core.talent_func import FunctionTalent
    ...
    ```

    Then you can start your application using `python run.py`
  - You can add a _run.py_ module to the _src/iotea/core_ and start your script by specifying it as a module<br />
    `python -m iotea.core.run`<br />
    You can use relative imports in your script<br />

    ```python
    from .talent_func import FunctionTalent
    ```
