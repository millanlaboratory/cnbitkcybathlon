#!/bin/bash

VERSION=0.8.3

echo "[gtk-dialog] - Downloading gtk-dialog-$VERSION"
wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/gtkdialog/gtkdialog-$VERSION.tar.gz
tar -xvf gtkdialog-$VERSION.tar.gz
cd gtkdialog-$VERSION/
./configure
make
sudo make install

echo "[gtk-dialog] - Removing installation directories and files"
cd ..
rm -r gtkdialog-$VERSION*
