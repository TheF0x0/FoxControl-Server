# FoxControl-Server
Local serial server application for the FoxControl project.

## Requirements
* An MCU board like an Arduino Pro (Mini) or an Arduino Uno (Any other ATmega328P based board should do)  
* An FTDI Dongle
* A 4-button remote controlled toy machine
* A computer running some flavor of Linux (tested against Ubuntu & PopOS)
* An OpenGL >= 3.3 capable GPU if you want to use the monitor UI
* An internet connection
* A [FoxControl Gateway](https://github.com/TheF0x0/FoxControl-Gateway) instance with SSL

## CLI Arguments
| Name            | Short Name | Description                                                            | Default Value     |
|-----------------|------------|------------------------------------------------------------------------|-------------------|
| **help**        | **h**      | Displays a CLI arguments help message.                                 |                   |
| **device**      | **d**      | Specifies the serial device to connect to.                             |                   |
| **rate**        | **r**      | Specifies the serial IO baud rate.                                     | 19200             |
| **address**     | **a**      | Specifies the address of the HTTP gateway to connect to.               |                   |
| **port**        | **p**      | Specifies the port of the HTTP gateway to connect to.                  | 443               |
| **updaterate**  | **u**      | Specifies the gateway fetch rate in milliseconds.                      | 250               |
| **certificate** | **c**      | Specifies the X509 certificate to use for gateway requests.            | ./certificate.crt |
| **password**    | **P**      | Specifies the password with which to authenticate against the gateway. |                   | 
| **monitor**     | **m**      | Opens the local monitor UI (Requires OpenGL >= 3.3).                   |                   |
| **verbose**     | **V**      | Enables verbose logging.                                               |                   |
| **version**     | **v**      | Shows version information.                                             |                   |

## Monitor UI
![image](https://user-images.githubusercontent.com/129870615/230422224-210a9977-629b-417b-b4f8-314c705bd574.png)
