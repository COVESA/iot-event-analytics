# Integration Test Naming Conventions

## Talents
Talents shall be named using camelCase  
e.g. `myTalentID`

### TestRunnerTalent
Shall be named like `testRunner[-\<SDK\>]`  
e.g. `testRunner-py`

### TestSetTalent
Shall be named like `testSet-\<scope\>[-\<SDK\>]`  
e.g. `testSet-sdk-js`

## Functions
Functions in Talents shall be named using camelCase  
e.g. `myFunction`

## Integration Test Function names
IoTEA Functions shall be named like `\<feature\>\<variation/case\>`  
e.g. `echoString`, `echoInteger`, `echoMixedList`  
**NOTE** Since a definition of a test using the test classes would need the expectedValue it would be not part of the function name. 

Internal implementation of this test classes should carry the prefix `test` using the naming convention of the related SDK.