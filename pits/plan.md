# PITS Project - Picture Image Transfer System

An IoT device used by photographers in the field for picture image transfer from their camera using FTP to the PITS server where processing and storage is done, and then waits for their phone to provide a hotspot and then use the PQTR user app to manage the production process.

We are now going to test it step by step.  

First step is to test init.  We will run init.sh with 1 as the argument.  After this we will connect to a bare pi server over ssh at z@192.168.1.52 and run wall.sh to get the firewall up.

Then we will run http.sh to get the web setup.  You are on the same network so we can test the page.

Then we will run apnt.sh, connect this machine to it, and then test ftp access.