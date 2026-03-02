#include "core.h"

/* Seed data — only used on first run before DB has any rows.
   After db_seed_products() runs these are written to the DB and
   subsequent startups load from DB instead. */

Category categories[] = {
    {1, "Food",   {60,100,60,255},  {255,255,255,255}, 0, 1},
    {2, "Drinks", {60,60,160,255},  {255,255,255,255}, 1, 1},
};

Product products[] = {
    {1, 1, "Burger",  1200.0f, {80,120,80,255},  {255,255,255,255}, 0, 1},
    {2, 1, "Pizza",   1500.0f, {80,120,80,255},  {255,255,255,255}, 1, 1},
    {3, 1, "Pasta",   1000.0f, {80,120,80,255},  {255,255,255,255}, 2, 1},
    {4, 1, "Rice",     800.0f, {80,120,80,255},  {255,255,255,255}, 3, 1},
    {5, 1, "Noodles",  900.0f, {80,120,80,255},  {255,255,255,255}, 4, 1},
    {6, 2, "Soup",     700.0f, {60,80,160,255},  {255,255,255,255}, 0, 1},
    {7, 2, "Coke",     300.0f, {60,80,160,255},  {255,255,255,255}, 1, 1},
    {8, 2, "Water",    150.0f, {60,80,160,255},  {255,255,255,255}, 2, 1},
};

int product_count  = 8;
int category_count = 2;
