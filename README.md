# Retail-Management-System
Implemented using socket programming and file locking mechanisms using system calls in C.


These are the code files in the project:
1. server.c
2. Customer.c
3. Admin.c
4. server.h

To run the code, compile server.c, Admin.c, and Customer.c and run the executables on
separate terminals to communicate between them. Only one server needs to be run, you can have multiple admins or customers runnnig concurrently. The code is structured such that the server.c code contains the definition of functions that are needed and sets up the server side of the
store. The Admin.c and Customer.c codes set up the client side codes of the store. The server.h code contains all the structure definitions and function declarations.

Concurrency is maintained throughout the code, by using file locking. The system call fcntl() was used for this. The data is all stored in files. The file created and used are the following:
1. Inventory.dat - stores inventory items (product ID, product Name, price)
2. Cart.dat - stores carts for each customer (the cart contains customer ID, number of unique products in the cart, and a list of cart items (product Name, price, quantity) )
3. QuantityLog.dat - stores a map between product names and their quantities

These files are locked when and where necessary while reading and writing. For communication between the executables, socket programming was used. The system calls used for this were: socket(), accept(), connect(), bind(), listen(), etc.

The admin can do the following: View the inventory, add items to the inventory, update an item in the inventory, delete an item in the inventory. A customer can do the following: View the inventory, add items to the their cart, edit their cart, delete an item form their cart, update their cart, buy the items in their cart.  
