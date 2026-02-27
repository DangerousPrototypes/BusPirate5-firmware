# Contributing.md 
This document contains all the rules and conventions that contributors must follow.

All the content in this document is copyrighted by the following authors:  
* Copyright (c) 2024 Lior Shalmay <liorshalmay1@gmail.com>

## Coding Convention
TBD

## Copyrights
You may copyright your contributed code, under the following rules: 

* You must add your name to the "copyright notices" section in `docs/licenses.md`.
* You must use the following signature `Copyright (c) <year> <your name> [<your email>]`, 
see the above copyright also as an example.
* If you contributed a new file, you may place the copyright notice at the top of the file.
* If you modify an existing file, add your copyright notice under the previous one, with the `Modified by` designation beforehand.
    * If there is no previous copyright notice, add your designation somewhere at the top of the file.
* You must not copyright code that is not yours!

## Licensing
You must contribute your code under the MIT license.

### 3rd party code
You may use 3rd party library in you contribution.
but then you must follow these rules:  

* You must make sure that the library's license is compatible with MIT license.
* You must document the library and the related copyright notices in `docs/licenses.md`.
* You must save a copy of the 3rd party library license in `docs/third_party_licenses`.
* You must document which source files in this project are using the library in `docs/licenses.md`.
* You must provide a way to fetch the library code from outside of this project.
* Depending on the license you may need to provide a way to compile this project without the library.


