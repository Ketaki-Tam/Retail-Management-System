#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>

#include "server.h"

struct InventoryItem Cart[MAX_CART_SIZE];


int main()
{
    int custID;
    printf("Enter your Customer ID: ");
    scanf("%d", &custID);
    
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

        int option=0, opt=0, custID=0; //to store which option was chosen, option in the edit part, customer id
        int pid=0, quan=0, price=0;
        char pname[100];
        char ch; //dummy character
        int size=0;  //number of products in the cart

        printf("Welcome to our store! What would you like to do today?\n");


        printf("Choose the option number:\n1) Display your cart\n2) Add item/items to cart\n3) Edit cart\n4) View Inventory\n5) Proceed to payment\n");
        scanf("%d", &option);
        option=option+5;  //to sync with the option numbers in the server program
        if(connect(sd,(struct sockaddr *)&serv,sizeof(serv)) != -1)
        {
            write(sd, &option, sizeof(int));
            if(option == 6)
            {
                write(sd, &custID, sizeof(int));  //sending the customer ID

                struct Cart tmp;
                
                read(sd,&tmp,sizeof(struct Cart));

                size=tmp.noOfProducts;
                if(size == 0)
                {
                    printf("\nNo products in the cart.\n\n");
                }
                else
                {
                    printf("ProductName   Price   Quantity\n");
                }

                for(int i=0;i<size;i++)
                {
                    printf("%s\t\t%d\t\t%d\n", tmp.cartItems[i].prodName, tmp.cartItems[i].price, tmp.cartItems[i].amount);  
                }



            }
            else if(option == 7)
            {
                write(sd, &custID, sizeof(int));  //sending the customer ID

                int found = 0; //a variable to indicate whether this productName already exists in the Cart or not
                printf("Enter the product name: ");
                scanf("%s", pname);
                printf("Enter the price: ");
                scanf("%d", &price);
                printf("Enter the quantity: ");
                scanf("%d", &quan);

                write(sd, &pname, sizeof(pname));  //sending the customer ID
                write(sd, &price, sizeof(int));  //sending the customer ID
                write(sd, &quan, sizeof(int));  //sending the customer ID



            }
            else if(option == 8)
            {
                int opt, done = 0;  //dine is for indicating that displaying the cart is done

                write(sd, &custID, sizeof(int));  //sending the customer ID

                struct Cart tmp;
                printf("ProductName   Price   Quantity\n");
                read(sd,&tmp,sizeof(struct Cart));

                size=tmp.noOfProducts;
                printf("number of products = %d\n", size);
                for(int i=0;i<size;i++)
                {
                    printf("%s\t\t%d\t\t%d\n", tmp.cartItems[i].prodName, tmp.cartItems[i].price, tmp.cartItems[i].amount);  
                }


                printf("Do you want to\n1) Edit the quantity of a product OR\n2) Delete a product\n");
                scanf("%d", &opt);

                if(opt == 1)
                {
                    // edit
                    int choice=1;
                    char pname[100];
                    int quan=0;
                    write(sd, &choice, sizeof(int));  //telling the server what to do
                    printf("Enter the name of product whose quantity you want to change:");
                    scanf("%s", pname);
                    printf("Enter the new quantity of the product: ");
                    scanf("%d", &quan);

                    write(sd, &pname, sizeof(pname));
                    write(sd, &quan, sizeof(int));

                }
                else if(opt == 2)
                {
                    // delete
                    int choice=2;
                    char pname[100];
                    write(sd, &choice, sizeof(int));  //telling the server what to do
                    printf("Enter the name if the product you want to delete from the cart: ");
                    scanf("%s", pname);

                    write(sd, &pname, sizeof(pname));



                }
                else
                {
                    printf("Please enter 1 or 2.\n");
                }




            }
            else if(option == 9)
            {
                // check inventory
                struct InventoryItem item;
                while(read(sd,&item,sizeof(struct InventoryItem)))
                {
                    printf("%d\t%s\t%d\n",item.productID,item.productName,item.price);
                }
            }
            else if(option == 10)
            {
                // buy items
                int totalcost=0; //total cost to be payed4
                int money=0;
                write(sd, &custID, sizeof(int));

                
                struct Cart tmp;
                char quan[2];
                read(sd,&quan,sizeof(quan));
                if(!strcmp(quan,"1"))
                {
                    printf("Not enough quantity. Try again later!\n\n");
                    continue;
                }

                read(sd,&tmp,sizeof(struct Cart));
                printf("Your cart:\n");
                printf("ProductName   Price   Quantity\n");

                size=tmp.noOfProducts;
                for(int i=0;i<size;i++)
                {
                    printf("%s\t\t%d\t\t%d\n", tmp.cartItems[i].prodName, tmp.cartItems[i].price, tmp.cartItems[i].amount);  
                }


                if(read(sd, &totalcost, sizeof(int)) == -1)
                {
                    printf("Purchase failed\n");
                }
                else
                {
                    while(1)
                    {
                        printf("The total cost is = %d\nPlease enter the amount to pay for the items bought:", totalcost );
                        scanf("%d", &money);
                        if(money == totalcost)
                        {
                            printf("Paid.\n");
                            write(sd, "0", sizeof("0"));
                            break;
                        }
                        else
                        {
                            printf("Not the right amount! Please enter again.\n");
                        }


                    }
                }

            }
            else
            {
                printf("Please enter 1, 2, 3, 4, 5\n");
            }
        }
        else
        {
            printf("Connection failed\n");
            perror("socket");
        }

        close(sd);

    }
    

    return 0;
}
