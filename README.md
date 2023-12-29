# hyprlang
The hypr configuration language is an extremely efficient, yet easy to work with, configuration language
for linux applications.

It's user-friendly, easy to grasp, and easy to implement.

## Example config

```ini
bakery {
    counter_color = rgba(ee22eeff)          # color by rgba()
    door_color = rgba(122, 176, 91, 0.1)    # color by rgba()
    dimensions = 10 20                      # vec2
    employees = 3                           # int
    average_time_spent = 8.13               # float
    hackers_password = 0xDEADBEEF           # int, as hex

    # nested categories
    secrets {
        password = hyprland                 # string
    }
}

# variable
$NUM_ORDERS = 3

cakes {
    number = $NUM_ORDERS                    # use a variable
    colors = red, green, blue               # string
}

# keywords, invoke your own handler with the parameters
add_baker = Jeremy, 26, Warsaw
add_baker = Andrew, 21, Berlin
add_baker = Koichi, 18, Morioh
```

## Docs

Soon:tm:
