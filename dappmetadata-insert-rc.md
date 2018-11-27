# Action - `{{ insert }}`

## Description

The {{insert}} action adds an entry on the {{dapp}} table including {{title}} of 33 chars or less, {{description}} of 1024 chars or less, {{source_code}} for a url of 128 chars or less, {{website}} for a URL of 128 chars or less, a {{logo}} for a URL of 128 chars or less, and up to 10 tags of 16 chars or less.

The contract also receive the {{ram_payer_opts}} and checks it before including the new entry in the {{dapp}} table.

The contract then procede to either update an existing registry or create a new registry on the {{dapp}} table.

