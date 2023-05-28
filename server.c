#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include "server.h"

struct InventoryInfo inv_handle;
struct CartInfo cart_handle;

int inv_create()
{
    int inv_fd = open("Inventory.dat",O_RDONLY | O_CREAT, 0644);
    int quan_fd = open("QuantityLog.dat",O_RDONLY | O_CREAT, 0644);
    if(inv_fd == -1 || quan_fd == -1)
    {
        perror("Error in creating the inventory file.\n");
        return SO_FILE_ERROR;
    }

    close(inv_fd);
    close(quan_fd);
}

int inv_open()
{
    int inv_fd = open("Inventory.dat",O_RDWR, 0644);
    int quan_fd = open("QuantityLog.dat",O_RDWR, 0644);

    if(inv_fd == -1 || quan_fd == -1)
    {
        perror("Error in opening the inventory file.\n");
        return SO_FILE_ERROR;
    }

    if (inv_handle.inv_status  == INVENTORY_OPEN)
    {
        return INVENTORY_ALREADY_OPEN;
    }
    else
    {
    	inv_handle.inv_entry_size = sizeof(struct InventoryItem);
    	inv_handle.inv_data_fd = inv_fd;
        inv_handle.inv_quantity_fd = quan_fd;
        inv_handle.inv_status = INVENTORY_OPEN;
        inv_handle.inv_count = 0;


		return inv_quantity_load();
	}


}

int inCount(char prodName[],struct InventoryCount count[])
{
    for(int i=0;i<MAX_INVENTORY_SIZE;i++)
    {
        if(!strcmp(count[i].prodName,prodName))
        {
            return i;
        }
    }

    return -1;
}

int inv_quantity_load()
{
    struct InventoryItem item;
    struct InventoryCount count[MAX_INVENTORY_SIZE];

    for(int i=0;i<MAX_INVENTORY_SIZE;i++)
    {
        strcpy(count[i].prodName,"");
        count[i].quantity = 0;
    }

    int i = 0;
    int index = 0;

    struct flock lock;  //setting a read lock on the whole file to check whether that product ID already has been added
    lock.l_type=F_RDLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
    fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);
    
    lseek(inv_handle.inv_data_fd,0,SEEK_SET);
    while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
    {
        if(item.valid == 1)
        {
            index = inCount(item.productName,count);
            if(index != -1)
            {
                count[index].quantity++;
            }
            else
            {
                for(int j = 0;j<MAX_INVENTORY_SIZE;j++)
                {
                    if(!strcmp(count[j].prodName,""))
                    {
                        strcpy(count[j].prodName,item.productName);
                        count[j].quantity = 1;
                        break;
                    }
                }
            }
            
            i++;
    
            if(i == MAX_INVENTORY_SIZE)
            {
                break;
            } 

        }
            
    }
    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);


    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
    fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);

    lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
    for(int i=0;i<MAX_INVENTORY_SIZE;i++)
    {
        write(inv_handle.inv_quantity_fd,&count[i],sizeof(struct InventoryCount));    }

    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);  //unlock

    return SO_SUCCESS;
    
}






int inv_add(int prodID,char* prodName,int price)
{
    if( inv_handle.inv_status == INVENTORY_CLOSED)
	{
        return SO_FILE_ERROR;
	}
    else
    {
        struct InventoryItem item;
        int offset = lseek(inv_handle.inv_data_fd,0,SEEK_END);
        int look_for_offset = 1;
        
        struct flock lock;  //setting a read lock on the whole file to check whether that product ID already has been added
        lock.l_type=F_RDLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);

        
        lseek(inv_handle.inv_data_fd,0,SEEK_SET);
        while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
        {
            if(item.productID == prodID && item.valid == 1)   //a valid product with that product ID has already been added
            {
                lock.l_type=F_UNLCK;    
                fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);
                return INVENTORY_ADD_FAILED;
            }
            else if(item.valid == 0 && look_for_offset == 1)
            {
                offset = lseek(inv_handle.inv_data_fd,-sizeof(struct InventoryItem),SEEK_CUR);
                look_for_offset = 0;
            }
        }
        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

        
        //writing to the inventory file
        if(offset == lseek(inv_handle.inv_data_fd,0,SEEK_END))  //if the data is being written to the end, the file size is increased by the amount needed
        {    
            ftruncate(inv_handle.inv_data_fd,offset+sizeof(struct InventoryItem));
        }
        

        item.productID = prodID;
        strcpy(item.productName,prodName);
        item.price = price;
        item.valid = 1;

        //setting a lock on the record where I want to write
        lock.l_type=F_WRLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=offset;
        lock.l_len=sizeof(struct InventoryItem);
        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);

        lseek(inv_handle.inv_data_fd,offset,SEEK_SET);
        int bytes = write(inv_handle.inv_data_fd,&item,inv_handle.inv_entry_size);
        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);  //unlock

        
        struct InventoryCount count_buf,count;
        int quan = 0;
        strcpy(count.prodName,prodName);
        
        lock.l_type=F_WRLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
        fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);

        lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
        while(read(inv_handle.inv_quantity_fd,&count_buf,sizeof(struct InventoryCount)))
        {

            if(!strcmp(count_buf.prodName,prodName))
            {
                lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
                quan = count_buf.quantity +1;
                count.quantity = quan;
                write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount));
                break;
            }
            else if(!strcmp(count_buf.prodName,""))   //find the first null entry, cuz a new product is added
            {
                lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
                count.quantity = 1;
                write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount));
                break;
            }
        }

        offset = lseek(inv_handle.inv_data_fd,0,SEEK_CUR);

        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);  //unlock
         

        return SO_SUCCESS;
    }
}

int inv_add_quantity(char prodName[],int quantity, int price)
{
    if( inv_handle.inv_status == INVENTORY_CLOSED)
	{
        return SO_FILE_ERROR;
	}
    else
    {
        struct InventoryItem item;
        int offset = lseek(inv_handle.inv_data_fd,0,SEEK_END);
        
        struct flock lock;  //setting a read lock on the whole file to check whether that product ID already has been added
        lock.l_type=F_RDLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);

        int max_prodID = 0;
        lseek(inv_handle.inv_data_fd,0,SEEK_SET);
        while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
        {
            if(item.valid == 1 && item.productID > max_prodID)   //a valid product with that product ID has already been added
            {
                max_prodID = item.productID;
                offset = lseek(inv_handle.inv_data_fd,0,SEEK_CUR);
            }
        }
        
        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

        
        //writing to the inventory file
        ftruncate(inv_handle.inv_data_fd,lseek(inv_handle.inv_data_fd,0,SEEK_END)+quantity*sizeof(struct InventoryItem));


        //setting a lock on the record where I want to write
        lock.l_type= F_WRLCK;
        lock.l_whence= SEEK_SET;
        lock.l_start= offset;
        lock.l_len= quantity*sizeof(struct InventoryItem);
        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);

        lseek(inv_handle.inv_data_fd,offset,SEEK_SET);

        //writing quantity number of products to the inventory
        int prodID = max_prodID + 1;
        for(int i=0;i<quantity;i++)
        {
            item.productID = prodID;
            strcpy(item.productName,prodName);
            item.price = price;
            item.valid = 1;
            write(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem));            prodID++;
        }

        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);  //unlock

        
        struct InventoryCount count_buf,count;
        int quan = 0;
        strcpy(count.prodName,prodName);
        
        lock.l_type=F_WRLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
        fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);

        lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
        while(read(inv_handle.inv_quantity_fd,&count_buf,sizeof(struct InventoryCount)))
        {

            if(!strcmp(count_buf.prodName,prodName))
            {
                lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
                quan = count_buf.quantity +quantity;
                count.quantity = quan;
                write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount));
                break;
            }
            else if(!strcmp(count_buf.prodName,""))   //find the first null entry, cuz a new product is added
            {
                lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
                count.quantity = quantity;
                write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount));
                break;
            }
        }

        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);  //unlock
        
        return SO_SUCCESS;
    }
}


int inv_update(int prodID,char* prodName, int price)
{
    struct InventoryItem item, old_item;
    if( inv_handle.inv_status == INVENTORY_CLOSED)
	{
		return SO_FILE_ERROR;
	}
    else
    {
        
        int deleted = 1;
        int offset = lseek(inv_handle.inv_data_fd,0,SEEK_END);

        struct flock lock;  //setting a read lock on the whole file to check whether that product ID already has been added
        lock.l_type=F_RDLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);
        lseek(inv_handle.inv_data_fd,0,SEEK_SET); //taking the cursor to the beginning
        while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
        {
            if(item.productID == prodID && item.valid == 1)
            {  
                deleted = 0;
                offset = lseek(inv_handle.inv_data_fd,-sizeof(struct InventoryItem),SEEK_CUR);
                lseek(inv_handle.inv_data_fd,offset,SEEK_SET);
                read(inv_handle.inv_data_fd,&old_item,sizeof(old_item));
                break;
            }
        }

        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

        if(deleted == 1)
        {
            return INVENTORY_ITEM_NOT_FOUND;
        }
        
        item.productID = prodID;
        strcpy(item.productName,prodName);
        item.price = price;
        item.valid = 1;

        lock.l_type=F_WRLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=offset;
        lock.l_len=sizeof(struct InventoryItem);
        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);

        lseek(inv_handle.inv_data_fd,offset,SEEK_SET);
        write(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem));
        
        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

        struct InventoryCount count_buf,count;
        int tasks_done = 0;
        strcpy(count.prodName,prodName);
        
        lock.l_type=F_WRLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
        fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);


        if(strcmp(old_item.productName,prodName))
        {
            while(read(inv_handle.inv_quantity_fd,&count_buf,sizeof(struct InventoryCount)))
            {
                if(!strcmp(count_buf.prodName,prodName))   //find the first null entry, cuz a new product is added
                {
                    lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
                    count.quantity = count_buf.quantity + 1;
                    write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount));
                    tasks_done++;

                }
                if(!strcmp(count_buf.prodName,old_item.productName))
                {
                    lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
                    count.quantity = count_buf.quantity - 1;
                    write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount));
                    tasks_done++;
                }
                if(tasks_done == 2)
                {
                    break;
                }
            }
        }
        
        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);  //unlock
        
    }

    return SO_SUCCESS;
}

int inv_delete(int prodID)
{
    struct InventoryItem item,old_item;
    int not_found = 1;
    int offset = lseek(inv_handle.inv_data_fd,0,SEEK_END);

    struct flock lock;  //setting a read lock on the whole file to check whether that product ID already has been added
    lock.l_type=F_RDLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
    fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);


    lseek(inv_handle.inv_data_fd,0,SEEK_SET); //taking the cursor to the beginning
    while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)) == sizeof(struct InventoryItem))
    {
        if(item.productID == prodID && item.valid == 1)
        {  
            not_found = 0;
            offset = lseek(inv_handle.inv_data_fd,-sizeof(struct InventoryItem),SEEK_CUR);
            old_item.productID = item.productID;
            strcpy(old_item.productName,item.productName);
            old_item.price = item.price;
            break;
        }
    }

    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

    if(not_found == 1)
    {
        return INVENTORY_ITEM_NOT_FOUND;
    }

    old_item.valid = 0;
    
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=offset;
    lock.l_len=sizeof(struct InventoryItem);
    fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);

    lseek(inv_handle.inv_data_fd,offset,SEEK_SET);
    write(inv_handle.inv_data_fd,&old_item,sizeof(struct InventoryItem));
    
    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

    struct InventoryCount count_buf,count;
    int tasks_done = 0;
    strcpy(count.prodName,old_item.productName);

    int quan=0; //for the quantity in the quatity count file
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
    fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);
    
    lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
    while(read(inv_handle.inv_quantity_fd,&count_buf,sizeof(struct InventoryCount)))
    {
        if(!strcmp(count_buf.prodName,old_item.productName))   //find the first null entry, cuz a new product is added
        {
            lseek(inv_handle.inv_quantity_fd,-sizeof(struct InventoryCount),SEEK_CUR);
            quan = count_buf.quantity;
            count.quantity = quan - 1;
            if(write(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount)) == -1)
            {
                printf(("Could not update the quantity in the quantity file successfully\n"));
            }
            else
            {
                printf("Updated the quantity file succcessfully\n");
            }
            break;
        }
        
    }

    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);  //unlock


    return SO_SUCCESS;

}

int inv_display(int nsd)
{
    if( inv_handle.inv_status == INVENTORY_CLOSED)
	{
        printf("Inventory is closed. Can't display inventory\n");
		return SO_FILE_ERROR;
	}
    else
    {
        struct InventoryItem item;
        struct flock lock;  //setting a read lock on the whole file to check whether that product ID already has been added
        lock.l_type=F_RDLCK;
        lock.l_whence=SEEK_SET;
        lock.l_start=0;
        lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);

        fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);


        lseek(inv_handle.inv_data_fd,0,SEEK_SET); //taking the cursor to the beginning
        int i = 0;
        while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
        {
            if(item.valid == 1)
            {  
                write(nsd,&item,sizeof(struct InventoryItem));   //this is writing to the socket not the file
            }
            i++;
            if(i == MAX_INVENTORY_SIZE)
            {
                break;
            }
        }

        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);

        return SO_SUCCESS;
    }
}

int inv_close()
{
    if( inv_handle.inv_status == INVENTORY_CLOSED)
	{
		return SO_FILE_ERROR;
	}
	else
    {
        close(inv_handle.inv_data_fd);
        close(inv_handle.inv_quantity_fd);
		inv_handle.inv_status = INVENTORY_CLOSED;
		return SO_SUCCESS;
    }
}


int cart_create()
{
    int cart_fd = open("Cart.dat",O_RDONLY | O_CREAT,0644);
    if(cart_fd == -1)
    {
        perror("Error in creating the cart file.\n");
        return SO_FILE_ERROR;
    }

    close(cart_fd);
}

int cart_open()
{
    int cart_fd = open("Cart.dat",O_RDWR,0644);

    if(cart_fd == -1)
    {
        perror("Error in opening the cart file.\n");
        return SO_FILE_ERROR;
    }

    if (cart_handle.cart_status  == INVENTORY_OPEN)
    {
        return INVENTORY_ALREADY_OPEN;
    }
    else
    {
    	cart_handle.cart_entry_size = sizeof(struct Cart);
    	cart_handle.cart_data_fd = cart_fd;
        cart_handle.cart_status = CART_OPEN;
        cart_handle.cart_count = 0; //GET RID OF THIS IF IT'S NOT BEING USED

	}

}

int cart_display(int custID, int nsd)
{
    int size=0; // number of products in the cart
    struct Cart cart;
    int result = cart_get_rec(custID,&cart);

    if(result != 0)
    {
        strcpy(cart.cartItems[0].prodName, "Nil");
        cart.cartItems[0].price=0;
        cart.cartItems[0].amount=0;
        cart.noOfProducts=1;
        write(nsd,&cart,sizeof(struct Cart));   //this is writing to the socket not the file
        return SO_SUCCESS;
    }
    else
    {
        write(nsd,&cart,sizeof(struct Cart));   //this is writing to the socket not the file 
    }

    
    
    
}

int cart_add(int custID,char prodName[],int price, int quantity)
{
    struct Cart temp;
    int found=0, size=0;  //to indicate if the product already exists in the cart
    int getResult = cart_get_rec(custID, &temp);
    if( getResult == 0)
    {
        //go through the products to see if this product name already exists
        for(int j=0; j<temp.noOfProducts;j++)
        {
            if(strcmp(temp.cartItems[j].prodName, prodName) == 0)
            {
                found = 1;
                int num = temp.cartItems[j].amount + quantity;
                temp.cartItems[j].amount = num;
                cart_update(&temp);
            }
        } 
        if(found == SO_SUCCESS)  // product name not found in the cart
        {
            size = temp.noOfProducts;
            strcpy(temp.cartItems[size].prodName, prodName);
            temp.cartItems[size].price=price;
            temp.cartItems[size].amount=quantity;
            int number=0;
            number=temp.noOfProducts+1;
            temp.noOfProducts=number;  //increasing the number of entries in the Cart
            cart_update(&temp);
        }
    }
    else if(getResult == CART_ITEM_NOT_FOUND) //REC_NOT_FOUND (customer id not found)
    {
        struct Cart rec;
        size = temp.noOfProducts;
        strcpy(rec.cartItems[0].prodName, prodName);
        rec.cartItems[0].price = price;
        rec.cartItems[0].amount = quantity;
        rec.custID = custID;
        rec.noOfProducts = 1;
        insert_cart(&rec);

    }

    
}


int insert_cart(struct Cart* cart)
{
    int bytes = 0;
    struct flock lock;
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    int end = lseek(cart_handle.cart_data_fd, 0, SEEK_END);
    lock.l_len= end;
    fcntl(cart_handle.cart_data_fd,F_SETLKW,&lock);
    ftruncate(cart_handle.cart_data_fd,end + sizeof(struct Cart));

    bytes = write(cart_handle.cart_data_fd,cart,sizeof(struct Cart));

    lock.l_type=F_UNLCK;
    fcntl(cart_handle.cart_data_fd, F_SETLK,&lock);  //unlock
    if(bytes == -1)
    {
        return CART_ADD_FAILED;
    }
    else
    {
        return SO_SUCCESS;
    }
        
}



int cart_get_rec( int id, struct Cart *cart ){
	 if(cart_handle.cart_status == CART_CLOSED)
    	{
        	return SO_FILE_ERROR;
    	}
    	else
    	{
            int count=0;
            struct Cart buf;

            lseek(cart_handle.cart_data_fd, 0, SEEK_SET);

            struct flock lock1;
            lock1.l_type=F_RDLCK;
            lock1.l_whence=SEEK_SET;
            lock1.l_start=0;
            lock1.l_len=lseek(cart_handle.cart_data_fd, 0, SEEK_END);
            fcntl(cart_handle.cart_data_fd,F_SETLKW,&lock1);
            lseek(cart_handle.cart_data_fd, 0, SEEK_SET);
            
            while(read(cart_handle.cart_data_fd, &buf, sizeof(struct Cart)) && count<MAX_CARTS)
            {
                if(buf.custID == id)
                {
                    cart->custID = buf.custID;
                    cart->noOfProducts = buf.noOfProducts;
                    for(int i=0;i<buf.noOfProducts;i++)
                    {
                        cart->cartItems[i] = buf.cartItems[i];
                    }
                    lock1.l_type=F_UNLCK;
                    fcntl(cart_handle.cart_data_fd,F_SETLK,&lock1);
                    return SO_SUCCESS;
                    

                }
                count++;
            }

            lock1.l_type=F_UNLCK;
            fcntl(cart_handle.cart_data_fd,F_SETLK,&lock1);

            

            

        return CART_ITEM_NOT_FOUND;

        }	
		
}

int cart_edit_quantity(int custID, char prodName[], int quantity)
{
    struct Cart temp;
    int found = 0; //to indicate if a product is found
    int getResult = cart_get_rec(custID, &temp);
    if( getResult == 0)  // the customer id exists
    {
        //go through the products to see if this product name already exists
        for(int j=0; j<temp.noOfProducts;j++)
        {
            if(strcmp(temp.cartItems[j].prodName, prodName) == 0)
            {
                found = 1;
                temp.cartItems[j].amount = quantity;
                cart_update(&temp);
            }
        } 
        if(found == 0)
        {
            printf("The product does not exist.\n");
        }  
    } 
    else
    {
        printf("The customer ID does not exist.\n");
    }
}

int cart_delete_item(int custID, char prodName[])
{
    struct Cart temp;
    int getResult = cart_get_rec(custID, &temp);
    int number; //temporary variable
    if( getResult == 0)  // the customer id exists
    for(int j=0; j<temp.noOfProducts;j++)
    {
        if(strcmp(temp.cartItems[j].prodName, prodName) == 0)
        {
           for(int i=j;i<temp.noOfProducts-1;i++)
           {
                temp.cartItems[i]=temp.cartItems[i+1];
           } 
           number = temp.noOfProducts;
           temp.noOfProducts=number - 1;
        }
    } 
    cart_update(&temp);

}



int cart_update(struct Cart* cart)
{
    struct flock lock;
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(cart_handle.cart_data_fd, 0, SEEK_END);
    fcntl(cart_handle.cart_data_fd,F_SETLKW,&lock);
    struct Cart buf;
    lseek(cart_handle.cart_data_fd, 0, SEEK_SET);
    while(read(cart_handle.cart_data_fd, &buf, sizeof(buf)))
    {
        if(buf.custID == cart->custID)
        {
            
            lseek(cart_handle.cart_data_fd, -sizeof(struct Cart), SEEK_CUR);
            write(cart_handle.cart_data_fd, cart, cart_handle.cart_entry_size);
            lock.l_type=F_UNLCK;
            fcntl(cart_handle.cart_data_fd, F_SETLK,&lock);
            return SO_SUCCESS;
            
        }
    }
    lock.l_type=F_UNLCK;
    fcntl(cart_handle.cart_data_fd, F_SETLK,&lock);
    return CART_ITEM_NOT_FOUND;
}



int cart_close()
{
    if( cart_handle.cart_status == CART_CLOSED)
	{
		return SO_FILE_ERROR;
	}
	else
	{
		close(cart_handle.cart_data_fd);
        close(cart_handle.cart_ndx_fd);
		cart_handle.cart_status = CART_CLOSED;
		return SO_SUCCESS;
	}  
}


int buyItems(int custID,int nsd)
{
    struct Cart cart;
    int i=0;

    //reading the cart data file to find this customer's cart
    struct flock lock,lock1;  //setting a read lock on the whole file
    lock.l_type=F_RDLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(cart_handle.cart_data_fd,0,SEEK_END);
    fcntl(cart_handle.cart_data_fd,F_SETLKW,&lock);

    lseek(cart_handle.cart_data_fd,0,SEEK_SET);
    while(read(cart_handle.cart_data_fd,&cart,sizeof(struct Cart)))    //to find the customer id
    {
        if(cart.custID == custID)
        {
            break;
        }

        i++;

        if(i == MAX_CARTS)
        {
            break;
        }
    }

    lock.l_type=F_UNLCK;    
    fcntl(cart_handle.cart_data_fd,F_SETLK,&lock);


    if(i == MAX_CARTS)
    {
        return BUY_FAILED;
    }


    //writing to the inventory file with the changes
    lock1.l_type=F_WRLCK;
    lock1.l_whence=SEEK_SET;
    lock1.l_start=0;
    lock1.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
    fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock1);


    //checking the quantity log to check whether enough quantity of items are present
    struct InventoryCount count;

    lock.l_type=F_RDLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
    fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);

    lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
    int j = 0,found = 0;


    for(int i=0;i<cart.noOfProducts;i++)
    {
        found = 0;
        j = 0;
        lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
        while(read(inv_handle.inv_quantity_fd,&count,sizeof(struct InventoryCount)))
        {
            if(!strcmp(count.prodName,cart.cartItems[i].prodName) && count.quantity < cart.cartItems[i].amount)
            {
                lock.l_type=F_UNLCK;    
                fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);
                lock1.l_type=F_UNLCK;
                fcntl(inv_handle.inv_data_fd,F_SETLK,&lock1);
                write(nsd,"1",sizeof("1"));
                return BUY_FAILED;
            }

            if(!strcmp(count.prodName,cart.cartItems[i].prodName))
            {
                found = 1;
                break;
            }

            j++;

            if(j == MAX_CART_SIZE)
            {
                break;
            }
        }
        if(found != 1)
        {
            lock.l_type=F_UNLCK;    
            fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);
            lock1.l_type=F_UNLCK;
            fcntl(inv_handle.inv_data_fd,F_SETLK,&lock1);
            write(nsd,"1",sizeof("1"));
            return BUY_FAILED;
        }

    }

    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);

    write(nsd,"0",sizeof("0"));

    //computing the total cost for the customer
    int totalcost=0;    //this will store the total cost for the cart
    int price=0, q=0;   //price and quantity of the cart items in the loop
    for(int i=0;i<cart.noOfProducts;i++)
    {
        price = cart.cartItems[i].price;
        q = cart.cartItems[i].amount;
        totalcost=totalcost+(price * q);
    }
    cart_display(custID,nsd);
    
    write(nsd,&totalcost,sizeof(int));  //sending totalcost to the customer. can proceed to payment. On the customer side the intrface to pay will appear, and this function will wait for approval to move on.
    char buf[2];
    read(nsd,&buf,sizeof(buf));  //get confirmation of the payment

    if(strcmp(buf,"0"))
    {
        lock.l_type=F_UNLCK;    
        fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);
        return BUY_FAILED;
    }


    //finding the items in the inventory to be deleted
    struct InventoryItem item;
    //int to_be_deleted[cart.noOfProducts];
    int index_to_be_deleted[cart.noOfProducts];
    int k = 0,no_of_loops = 0;


    


    // lock.l_type=F_RDLCK;
    // lock.l_whence=SEEK_SET;
    // lock.l_start=0;
    // lock.l_len=lseek(inv_handle.inv_data_fd,0,SEEK_END);
    // fcntl(inv_handle.inv_data_fd,F_SETLKW,&lock);
	

    for(int i=0;i<cart.noOfProducts;i++)
    {
        found = 0;
        no_of_loops = 0;
        lseek(inv_handle.inv_data_fd,0,SEEK_SET);
        for(int j = 0;j<cart.cartItems[i].amount;j++)
        {
            while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
            {
                no_of_loops = no_of_loops + 1;
                if(!strcmp(cart.cartItems[i].prodName,item.productName) && item.valid == 1)
                {
                    //to_be_deleted[k] = item.productID;
                    index_to_be_deleted[k] = no_of_loops - 1;
                    k = k+1;
                    break;
                }
                
                if(no_of_loops == MAX_INVENTORY_SIZE)
                {
                    break;
                }
            }
        }  

    }

    // lock.l_type=F_UNLCK;    
    // fcntl(inv_handle.inv_data_fd,F_SETLK,&lock);  //unlock


    int offset=-1;
    for(int i=0;i<k;i++)
    {
        offset = index_to_be_deleted[i]*sizeof(struct InventoryItem);
        lseek(inv_handle.inv_data_fd, offset,SEEK_SET);
        read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem));
        item.valid = 0;
        lseek(inv_handle.inv_data_fd,-sizeof(struct InventoryItem),SEEK_CUR);
        write(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem));

    }

    

    struct InventoryCount count1[MAX_INVENTORY_SIZE];

    for(int i=0;i<MAX_INVENTORY_SIZE;i++)
    {
        strcpy(count1[i].prodName,"");
        count1[i].quantity = 0;
    }

    i = 0;
    int index = 0;
    
    lseek(inv_handle.inv_data_fd,0,SEEK_SET);
    while(read(inv_handle.inv_data_fd,&item,sizeof(struct InventoryItem)))
    {
        if(item.valid == 1)
        {
            index = inCount(item.productName,count1);
            if(index != -1)
            {
                count1[index].quantity++;
            }
            else
            {
                for(int j = 0;j<MAX_INVENTORY_SIZE;j++)
                {
                    if(!strcmp(count1[j].prodName,""))
                    {
                        strcpy(count1[j].prodName,item.productName);
                        count1[j].quantity = 1;
                        break;
                    }
                }
            }
            
            i++;
    
            if(i == MAX_INVENTORY_SIZE)
            {
                break;
            } 

        }
            
    }


    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(inv_handle.inv_quantity_fd,0,SEEK_END);
    fcntl(inv_handle.inv_quantity_fd,F_SETLKW,&lock);

    lseek(inv_handle.inv_quantity_fd,0,SEEK_SET);
    for(int i=0;i<MAX_INVENTORY_SIZE;i++)
    {
        write(inv_handle.inv_quantity_fd,&count1[i],sizeof(struct InventoryCount));    }

    lock.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_quantity_fd,F_SETLK,&lock);  //unlock


    lock1.l_type=F_UNLCK;    
    fcntl(inv_handle.inv_data_fd,F_SETLK,&lock1);  //unlock

    // emptying the cart
    struct Cart cart_buf, new_cart;
    int c = 0;

    lock.l_type=F_RDLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=0;
    lock.l_len=lseek(cart_handle.cart_data_fd,0,SEEK_END);
    fcntl(cart_handle.cart_data_fd,F_SETLKW,&lock);

    lseek(cart_handle.cart_data_fd,0,SEEK_SET);
    while(read(cart_handle.cart_data_fd,&cart_buf,sizeof(struct Cart)))
    {
        if(cart_buf.custID == custID)
        {
            offset = lseek(cart_handle.cart_data_fd,-sizeof(struct Cart),SEEK_CUR);
            break;
        }
        c++;
        if(c == MAX_CARTS)
        {
            break;
        }
    }



    
    int receipt = open("Receipt.txt",O_RDWR | O_TRUNC | O_CREAT, 0666);
    if(receipt == -1)
    {
        printf("Error opening receipt file.\n");
    }
    else
    {
        
        struct flock lock1;
        lock1.l_type=F_WRLCK;
        lock1.l_whence=SEEK_SET;
        lock1.l_start=0;
        lock1.l_len=lseek(receipt,0,SEEK_END);
        fcntl(receipt,F_SETLKW,&lock1);
        
        lseek(receipt,0,SEEK_SET);

        char customerID[100];
        sprintf(customerID,"%d",custID);
        write(receipt,"Customer ID = ",sizeof("Customer ID = "));
        write(receipt,customerID,sizeof(customerID));
        write(receipt,"\n",sizeof("\n"));

        char cost[100];
        char amount[100];
        char total[100];

        for(int i=0;i<cart.noOfProducts;i++)
        {
            write(receipt,cart.cartItems[i].prodName,sizeof(cart.cartItems[i].prodName));
            write(receipt,"\t",sizeof("\t"));
            sprintf(cost,"%d",cart.cartItems[i].price);
            write(receipt,cost,sizeof(cost));
            write(receipt,"\t",sizeof("\t"));
            sprintf(amount,"%d",cart.cartItems[i].amount);
            write(receipt,amount,sizeof(amount));
            write(receipt,"\n",sizeof("\n"));

        }

        sprintf(total,"%d",totalcost);
        write(receipt,"Total cost = ",sizeof("Total cost = "));
        write(receipt,total,sizeof(total));
        write(receipt,"\n",sizeof("\n"));
        write(receipt,"\n",sizeof("\n"));

        lock1.l_type = F_UNLCK;
        fcntl(receipt,F_SETLK,&lock1);
    }
    

    new_cart.custID  = custID;
    new_cart.noOfProducts = 0;

    for(int i=0;i<MAX_CART_SIZE;i++)
    {
        new_cart.cartItems[i].amount = 0;
        strcpy(new_cart.cartItems[i].prodName,"");
        new_cart.cartItems[i].price = 0;
    }

    lock.l_type=F_UNLCK;    
    fcntl(cart_handle.cart_data_fd,F_SETLK,&lock);  //unlock

    //writing the empty cart back to the cart file
    lock.l_type=F_WRLCK;
    lock.l_whence=SEEK_SET;
    lock.l_start=offset;
    lock.l_len=sizeof(struct Cart); 
    fcntl(cart_handle.cart_data_fd,F_SETLKW,&lock);

    lseek(cart_handle.cart_data_fd,offset,SEEK_SET);
    write(cart_handle.cart_data_fd,&new_cart,sizeof(struct Cart));

    lock.l_type=F_UNLCK;    
    fcntl(cart_handle.cart_data_fd,F_SETLK,&lock);  //unlock




    return SO_SUCCESS;

}




int main()
{
    inv_create();
    inv_open();
    cart_create();
    cart_open();

    printf("Inventory successfully setup.\n");
    //setting up the socket
    struct sockaddr_in serv,cli; 
    int portno = PORT_NO;

    int sd = socket(AF_INET,SOCK_STREAM,0);
    if (sd == -1){
        perror("Error: ");
        return -1;
    }
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(portno);

    int opt = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        perror("Error: ");
        return -1;
    }

    if(bind(sd,(struct sockaddr *)&serv,sizeof(serv)) == -1)
    {
        perror("Bind error:\n");
        return -1;
    }

    if(listen(sd,100) == -1)
    {
        perror("Listen erro:\n");
        return -1;
    }

    int nsd;
    int size = sizeof(cli);
    int option;

    while(1)
    {
            nsd = accept(sd,(struct sockaddr *)&cli,&size);

            if(nsd != -1)
            {
                printf("Successful accept\n");
                if(!fork())
                {
                    // close(sd);

                    read(nsd,&option,sizeof(option));
                    if(option == 1)
                    {
                        inv_display(nsd);
                    }
                    else if(option == 2)
                    {
                        struct InventoryItem item;
                        char response[100];
                        read(nsd,&item,sizeof(struct InventoryItem));
                        if(inv_add(item.productID,item.productName,item.price) == SO_SUCCESS)
                        {
                            strcpy(response,"Successfully added the product.");
                        }
                        else
                        {
                            strcpy(response,"Could not add the product. Sorry!");
                        }
                        write(nsd,response,sizeof(response));

                    }
                    else if(option == 3)
                    {
                        char response[100];
                        int prodID;
                        read(nsd,&prodID,sizeof(prodID));

                        if(inv_delete(prodID) == SO_SUCCESS)
                        {
                            strcpy(response,"Successfully deleted the product.");
                        }
                        else
                        {
                            strcpy(response,"Could not add the product. Sorry!");
                        }
                        write(nsd,response,sizeof(response));

                    }
                    else if(option == 4)
                    {
                        struct InventoryItem item;
                        char response[100];
                        read(nsd,&item,sizeof(struct InventoryItem));
                        if(inv_update(item.productID,item.productName,item.price) == SO_SUCCESS)
                        {
                            strcpy(response,"Successfully updated the product.");
                        }
                        else
                        {
                            strcpy(response,"Could not update the product. Sorry!");
                        }
                        write(nsd,response,sizeof(response));

                    }
                    else if(option == 5)
                    {
                        char prodName[100];
                        int quantity;
                        int price;

                        read(nsd,prodName,sizeof(prodName));
                        read(nsd,&quantity,sizeof(quantity));
                        read(nsd,&price,sizeof(price));

                        if(inv_add_quantity(prodName,quantity,price) == SO_SUCCESS)
                        {
                            write(nsd,"0",sizeof("0"));
                        }
                        else
                        {
                            write(nsd,"1",sizeof("1"));
                        }
                    }
                    else if(option == 6)   //customer options
                    {
                        int custID;
                        read(nsd,&custID,sizeof(int));    //taking in the customer ID
                        cart_display(custID, nsd);
                    }
                    else if(option == 7)
                    {
                        int custID;
                        int quan=0, price=0;
                        char pname[100];

                        read(nsd,&custID,sizeof(int));    //taking in the customer ID
                        // now read the product name, price and quantity and pass it to the add function
                        read(nsd,&pname,sizeof(pname));    //taking in the product name
                        read(nsd,&price,sizeof(int));    //taking in the price
                        read(nsd,&quan,sizeof(int));    //taking in the quantity

                        cart_add(custID, pname, price, quan);
                    }
                    else if(option == 8)
                    {
                        int custID;
                        int opt;
                        read(nsd,&custID,sizeof(int));    //taking in the customer ID
                        //displaying the cart for the customer
                        cart_display(custID, nsd);
                        
                        read(nsd,&opt,sizeof(int)); 
                        if(opt == 1)
                        {
                            char pname[100];
                            int quan=0;

                            read(nsd, &pname, sizeof(pname));
                            read(nsd, &quan, sizeof(int));
                            cart_edit_quantity(custID, pname, quan);
                        }   
                        if(opt == 2)
                        {
                            char pname[100];
                            read(nsd, &pname, sizeof(pname));
                            cart_delete_item(custID, pname);
                        }

                    }
                    else if(option == 9)
                    {
                        inv_display(nsd);
                    }
                    else if(option == 10)
                    {
                        int custID;
                        read(nsd,&custID,sizeof(int));    //taking in the customer ID

                        buyItems(custID,nsd);
                    }
                    //ans=0;  //resetting
                    close(nsd);
                }
                else
                {
                    //ans=0;
                    close(nsd);
                }
            }
            else
            {
                printf("Unsuccessful accept\n");
                perror("socket");
                break;
            }
            
        // }
    }

    return 0;
}

