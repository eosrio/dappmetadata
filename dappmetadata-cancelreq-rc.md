# Action - `{{ cancelreq }}`

## Description

The {{cancelreq}} action allows a {{payer}} to cancel a auditing request.

It first uthenticate the {{payer}} and request.

If the request was already accepted and the deadline for submission have not been reached it should return an error.

Else the {{payer}} is refunded and the request deleted.

