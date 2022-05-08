# NNR
NEU NFC Reader (ACR122U).

## requirements
```sh
sudo pacman -S pcsclite
sudo systemctl start pcscd.service
```

## build
```sh
gcc -I/usr/include/PCSC -lpcsclite nnr.c main.c -o NNR
```
