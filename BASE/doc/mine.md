# Notes

These are my notes - don't touch!

## Host

The host is a hardened host. It runs nginx as the bastion host and needs:

curl


## JWTA

JWTA is the JWT auth component.  It runs as a standalone service and manages User account creation and resets etc.
It's interface is RPC
It provides JWT tokens
Its interface is JRPC = JSON-RPC 2.0
It uses mailgun to provide MOTP = Mail One time pass codes.
It has 4 Roles in order of access level = NONE PLAY HERO PQTR.  PQTR is the admin role.
It has main RPC methods:

| Method | Params             | Auth | Description        |
| ------ | ------------------ | ---- | ------------------ |
| find   | email              | PQTR | Find user by email |
| give   | jwt, user_id, role | PQTR | Set user's role    |
| take   | jwt, user_id       | PQTR | Reset role to NONE |
| lock   | jwt, user_id       | PQTR | Lock account       |
| free   | jwt, user_id       | PQTR | Unlock account     |
| drop   | jwt, user_id       | PQTR | Delete user        |
| info   | jwt                | PQTR | System stats       |



## Add Mailgun API key
login to host and add the mailgun auth@pqtr.ai key to the keyring using:

  keyctl add user "jwta:mailgun_api_key" "YOUR_API_KEY" @u

## Optional: override other config values from keyring
  keyctl add user "jwta:mailgun_domain" "pqtr.ai" @u
  keyctl add user "jwta:mailgun_from" "PQTR Auth <auth@pqtr.ai>" @u
  keyctl add user "jwta:mailgun_region" "eu" @u
The key is in the One Password Notes for Mailgun@PQTR


Then - send.sh jwta