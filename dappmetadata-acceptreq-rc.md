# Action - `{{ acceptreq }}`

## Description

The {{acceptreq}} action check if the {{o.accepted}} value on the {{requests_table}} is still false meaning that the request have not been accepted yet

if so it authenticates the {{val_account}} and check if it is registered as validator, 

if so the votes assigned to the validator are update. 

Then the contract checks if the sum of votes of the BPs approving the validators surpass the threshold defined in the request. 

If so the contract update the request table {{o.accepted}} to true and the {{o.accepted_on}} to the current time.
