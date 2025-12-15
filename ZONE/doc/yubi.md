
---

# Core Principle

In computer space there are no humans — only computers talking to computers.
What humans do is set up processes so that the actions of a device can be trusted as actions of the right person.

When a device connects to a server using SSH, it presents credentials. But how do we know those credentials are under the control of the correct, authorized human?

A password alone isn’t enough. Instead, we place those credentials on a hardware security module, such as a YubiKey, which is assumed to be in the human’s possession. The device can only use the credentials when the human approves — by entering a PIN or touching the key.

This way, the release of credentials can be taken as proof that the correct human has authorized the action in computer space.

or in simple terms:

SSH proves a device has credentials; a YubiKey proves the right human is controlling them.

---

👉 Do you want me to also **condense this into a one-liner** (like a slogan/summary) that captures the whole principle in a single sentence?


Got it 👍 — here’s a straight-to-the-point executive summary for someone new:

---

# 🔑 YubiKey + OpenPGP (for SSH/GPG) — Executive Summary

* **YubiKey** = a tiny hardware security key.

  * It has different “apps” inside (OpenPGP, PIV, FIDO2, OTP, OATH).
  * Each app is like a smart card with its own PINs and keys.

* **OpenPGP app** is what you use for **SSH and GPG keys**.

  * It holds private keys **inside the key** — they can’t be copied or stolen.
  * GPG/SSH ask the YubiKey to sign/authenticate → you touch the key to confirm.

* **PINs** protect use:

  * User PIN (default `123456`) → needed to use the key (SSH login, signing).
  * Admin PIN (default `12345678`) → needed for setup (generate keys, change config).
  * Reset Code (optional) → recovery if you forget the User PIN.

* **Reset anytime** → If you forget PINs or mess up, you can wipe the OpenPGP app (`ykman openpgp reset`). This restores defaults and clears keys.

* **Workflow for SSH use** (best practice):

  1. Reset OpenPGP to start clean.
  2. Change User PIN + Admin PIN + set Reset Code.
  3. Generate an **Ed25519 key on the YubiKey** (private key never leaves).
  4. Export the **public key** and copy it to servers (`~/.ssh/authorized_keys`).
  5. Use SSH — system prompts for PIN and touch on the YubiKey.

* **Why it’s good**:

  * Hardware-isolated: malware on your PC can’t steal the private key.
  * Phishing-resistant: key won’t sign bogus requests.
  * Portable: one device unlocks SSH/GPG access on any computer.

---

👉 In short: **Use YubiKey’s OpenPGP app to hold your SSH key. Reset once, set strong PINs, generate the key on the device, and use the public half on your servers. Your private key never leaves the stick.**

# Yubikey Manager Setup

Run ykman in a python env


source venv/bin/activate
(to quite just type deactivate)

sudo apt update
sudo apt install libusb-1.0-0 pcscd libpcsclite1 libpcsclite-dev python3.11-dev -y

---

# OpenPGP card setup

Do a hard reset on the device.  Note the admin and user keys.

` ykman openpgp reset

Set the admin pin.  This is a master control of the user pin.  We don't use that feature so you can set it and be your own admin.  Note that you need to use the default admin PIN showed after the reset process.

` ykman openpgp access change-admin-pin

Set your user pin for operations.    Note that you need to use the default user PIN showed after the reset process.

` ykman openpgp access change-pin

Now set your name details.  Note that whenever you enter the "admin" command you will need to enter your ADMIN pin (not user PIN).

` gpg --card-edit
` admin
` name
` quit

Check the details

` gpg --card-status

# OpenPGP SSH Key setup

Ensure the PGP agent is running for your environment

` mkdir -p ~/.gnupg
` echo "enable-ssh-support" >> ~/.gnupg/gpg-agent.conf
` gpgconf --kill gpg-agent
` gpgconf --launch gpg-agent

We need to tell gpg to generate edwards curve keys (ed25519).  This will need your ADMIN PIN.

` gpg --card-edit
` admin
` key-attr

Run gpg to setup the keys.  Note you will use your USER PIN.  Use 0 for never expires.  Select keep an offline copy.  Choose a strong password suitable for the file to be found but not hacked.

` gpg --card-edit
` admin
` generate

Once generated, you can now see the key information. The hash is the key id and can be shown at any time with this command.

gpg --list-keys

Congratulations - you now have an edwards curve key.  Now we want to set the touch on option so that when asked to use a key on an unlocked yubikey, you have to touch the key button.

You may have to free the device from its current gpg session.

` gpgconf --kill scdaemon
` gpgconf --kill gpg-agent
` sudo systemctl restart pcscd

Then using your ADMIN PIN:

` # require touch to sign (S)
` ykman openpgp keys set-touch sig on   
` # require touch to authenticate (SSH) (A)
` ykman openpgp keys set-touch aut on   
` # require touch to decrypt (E)
` ykman openpgp keys set-touch enc on   

We now move to the SSH setup.  Here we have the Yubikey device with your ED25519 keys on it.  The PGP agent is running and can read it. Now we have to get SSH to use the PGP agent.  Add the following line to your ~/.bashrc file so that when your bash terminal starts SSH links itself to GPG.  After you have added it, close your terminal session and open it again so your new bash session loads the profile.

` export SSH_AUTH_SOCK=$(gpgconf --list-dirs agent-ssh-socket)

Run the following and you should see your key available.

` ssh-add -L
