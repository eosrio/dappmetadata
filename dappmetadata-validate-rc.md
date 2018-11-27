# Action - `{{ validate }}`

## Description

The {{validate}} contract first check the existence and validity of the request and pay the bounty to the {{validator}}

Then it adds a register to the {{validations_table}} containing the {{validator}} name, code hash, version (if updating existing validation), tags, notes and a timestamp. 
