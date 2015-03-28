#!/bin/sh
#JPR version 1.1 August 8 2010
DIDIWIKIHOME=
datadir=~/.didiwiki
scripts=~/scripts
images=$datadir/images
zipped=$datadir/files
permission=$datadir/permission
login=$datadir/.login.txt
styles=$datadir/styles.css

echo "This script: finish_install.sh is located in the ciwiki package."
echo "I am going to install some extra files for CiWiki. If you do not want press ctrl-c to quit or Enter to continue."
read a

if ! [ -d "src" ]
then 
  echo "Please run this script from ciwiki-xxx directory."
  read a
  exit 0
fi

if [ $DIDIWIKIHOME ]
then
  echo -n "Are you going to use $DIDIWIKIHOME as working directory (y)? "
  read a
  if [ $a = "y" ]
  then
    echo "Okay!"
    datadir=$DIDIWIKIHOME
  else
    echo "Remove $DIDIWIKIHOME if do not want to use it."
    exit 0;
  fi
else
  echo -n "Are you going to use the default directory $datadir (y)? "
  read a
  if ! [ $a ] || [ $a = "y" ]
  then
    if [ -d "$datadir" ]
    then
      echo "Okay! Folder $datadir exists."
    else
      echo "I create the folder $datadir."
      mkdir -v "$datadir"
    fi
  else
    echo -n "Please enter the working directory: "
    read wdir
    echo -n " '$wdir'  Is it correct (y)? "
    read a
    if ! [ $a ] ||  [ $a = "y" ]
    then
      datadir=$wdir
      if [ -d "$datadir" ]
      then
        echo "Okay! Folder $datadir exists."
      else
        echo "I create the folder $datadir."
        mkdir -v "$datadir"
      fi
    else
      echo "Please run again this script."
      exit 0;
    fi
  fi
fi

echo "I will install styles.css in $datadir if $styles does not exist."
echo "sh files are copied in $scripts and png images are copied in $images."
echo ".login.txt is moved in $permission for previous version < 1.7"
echo "Hit Enter to start or Ctrl-c to exit this script."
read a

# Install css styles
if [ -f "$styles" ]
then 
  echo "File styles.css already exist in ~/.didiwiki/"
  echo "Look at the differences between the file in package and intalled:"
  diff -s styles.css $styles
  echo
else
  echo "I am copying styles.css in ~/.didiwiki/"
  cp -v styles.css $datadir
fi

#Install scripts
if [ -d "$scripts" ]
then 
  echo "Folder $scripts exists - I am copying sh scripts."
  cp -v scripts/*.sh $scripts/
else
  echo "I am creating the folder $scripts."
  mkdir -v $scripts
  echo "I am copying sh scripts."
  cp -v scripts/*.sh $scripts/
fi

#Install images
if [ -d "$images" ]
then 
  if [ -f "images/ciwiki.png" ]
  then
    echo "Folder $images exists - I am copying png pictures"
    cp -v images/*.png $images/
  fi
else
  echo "I am creating the folder $images."
  mkdir -v $images
  if [ -f "images/ciwiki.png" ]
  then
    echo "I am copying pictures."
    cp -v images/*.png $images/
  fi
fi

#Install zip 
if [ -d "$zipped" ]
then 
  if [ -f "files/index.zip" ]
  then
    echo "Folder $files exists - I am copying zip files"
    cp -v files/*.zip $zipped/
  fi
else
  echo "I am creating the folder $files."
  mkdir -v $zipped
  if [ -f "files/index.zip" ]
  then
    echo "I am copying zip files."
    cp -v files/*.zip $zipped/
  fi
fi

#Move .login.txt for version < 1.7
if [ -f "$datadir/.login.txt" ]
then 
  if [ -d "$permission" ]
    then 
      echo "I am going to move .login.txt in $permission"
      mv $datadir/.login.txt $permission/.login.txt
    else
      echo "I am creating $permission and moving .login.txt in $permission"
      mkdir -v $permission
      mv $datadir/.login.txt $permission/.login.txt
  fi
fi
echo
echo "You will need sSMTP to notify new users by email. I am checking if you have it:"
if [ -f "/usr/bin/ssmtp" ] ||  [ -f "/usr/sbin/ssmtp" ] ||  [ -f "/sbin/ssmtp" ] || [ -f "/usr/local/bin/ssmtp" ] ||  [ -f "/usr/local/sbin/ssmtp" ]
then
  echo "sSMTP is already installed." 
  echo "Don't forget to configure it: /etc/ssmtp/ssmtp.conf"
else
  echo "You will have to install sSMTP  (sudo apt-get install ssmtp for Debian)."
  echo "Don't forget to configure ssmtp.conf: /etc/ssmtp/ssmtp.conf"
fi
echo "---- End of this script ----"
echo "Hit Enter to exit"
read a
