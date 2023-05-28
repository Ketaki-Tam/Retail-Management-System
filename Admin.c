#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>

#include "server.h"

int main()
{
    while(1)
    {
        struct sockaddr_in serv;
        int sd = socket(AF_INET,SOCK_STREAM,0);

        if (sd == -1){
            perror("Error: ");
            return -1;
        }

        int portno = PORT_NO;
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = INADDR_ANY;
        serv.sin_port = htons(portno);

        printf("Hello! What would you like to do today?\n1)View the inventory\n2)Add an item\n3)Delete an item\n4)Update an item\n5)Add item by quantity\n");
        int option;
        scanf("%d",&option);
        int size = sizeof(option);
        if(connect(sd,(struct sockaddr *)&serv,sizeof(serv)) != -1)
        {
            write(sd,&option,size);
            if(option == 1)
            {
                struct InventoryItem item;
                while(read(sd,&item,sizeof(struct InventoryItem)))
                {
                    printf("%d\t%s\t%d\n",item.productID,item.productName,item.price);
                }
            }
            else if(option == 2)
            {
                printf("Enter the product ID, product name and the price of the product: \n");
                int prodID;
                char prodName[100];
                int price;
                scanf("%d",&prodID);
                scanf("%s",prodName);
                scanf("%d",&price);

                struct InventoryItem item ;
                item.productID = prodID;
                strcpy(item.productName,prodName);
                item.price = price;

                write(sd,&item,sizeof(item));
                char response[100];
                read(sd,response,sizeof(response));
                printf("%s\n",response);
            }
            else if(option == 3)
            {
                printf("Enter the productID that you wan't to remove:\n");
                int prodID;
                scanf("%d",&prodID);

                write(sd,&prodID,sizeof(prodID));
                char response[100];
                read(sd,response,sizeof(response));
                printf("%s\n",response);
            }
            else if(option == 4)
            {
                printf("Enter the product ID, product name and the price of the product you want to update (updated based on productID): \n");
                int prodID;
                char prodName[100];
                int price;
                scanf("%d",&prodID);
                scanf("%s",prodName);
                scanf("%d",&price);

                struct InventoryItem item ;
                item.productID = prodID;
                strcpy(item.productName,prodName);
                item.price = price;

                write(sd,&item,sizeof(item));
                char response[100];
                read(sd,response,sizeof(response));
                printf("%s\n",response);
            }
            else if(option == 5)
            {
                printf("Enter the productName, quantity and price of the product you want to add:\n");
                char prodName[100];
                int quantity;
                int price;
                scanf("%s",prodName);
                scanf("%d",&quantity);
                scanf("%d",&price);

                write(sd,prodName,sizeof(prodName));
                write(sd,&quantity,sizeof(quantity));
                write(sd,&price,sizeof(price));

                char str[2];
                read(sd,&str,sizeof(str));

                if(!strcmp(str,"0"))
                {
                    printf("Successfully added.\n");
                }
                else
                {
                    printf("Couldn't add the products.\n");
                }


            }
        }
        else{
            printf("Unsuccessful connection.\n");
            perror("");
        }

        close(sd);
    }
    
    
    
    
    
}