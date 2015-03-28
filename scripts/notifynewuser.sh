#!/bin/sh
# This simple bash script will send the validation code to the new users
# Require ssmtp (relay email to your smtp)
# if newwikiusers.txt exists, then it's renamed.
# Email, name,pwd and code are extracted and sent with ssmtp
# The result file newusersnotified.txt is updated
# JP Redonnet May 2010 - inphilly@gmail.com
# Rev 1.3 October 31, 2013
# newwikiusers.txt should be in the home dir
file1=~/newwikiusers.txt
# temporary file
file2=~/newwikiusers.temp
# list of users notified
file3=~/newusersnotified.txt

#wiki address, we need the ip address and the port 
#!!! don't forget to update the port if you changed it (default is 8000)
#we will create a direct link to validate a new registration

#extract the ip address of the server
str=`ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1'`

wikiaddr='http://'${str%% *}':8000'
#!!! Problem? try to uncomment the line below, change your_inet_address with your inet address and comment the line above
#wikiaddr='http://your_inet_address:8000' 

# Subject, body
subject='Your CiWiki Access Code\r'
body='Hello\r\rTo validate your new account;
      Please click on the direct link below,
      or in the Wiki, click on login, choose new account,
      Reenter your username, password, and email,
      Enter your access code.\r'
thanks='\rThank you!\r'

if [ -f "$file1" ]
then
  mv -f $file1 $file2
  while read line
  do
    email=${line#*M:}
    email=${email%% *}
    usr=${line#*U:}
    usr=${usr%% *}
    pwd=${line#*P:}
    pwd=${pwd%% *}
    code=${line#*C:}
    direct=$wikiaddr'/Login?rac='$email,$code,$usr,$pwd

    date >> $file3
    echo "$line" >> $file3
    echo "Subject:$subject\r \
    $body\r \
    Your username:$usr\r \
    Your password:$pwd\r \
    Your access code:$code\r \
    Direct link: $direct\r \
    $thanks\r"|sendmail $email >> $file3
  done < $file2
fi
exit 0
