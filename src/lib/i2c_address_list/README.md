# I2C_Addresses

List of I2C addresses for various devices by address range.

## Adding New I2C Addresses

To add or update an I2C address, you can submit a pull request this repository. For
instructions on how to submit a pull request (and working with `git` in general),
see [this Learn guide on the subject](https://learn.adafruit.com/contribute-to-circuitpython-with-git-and-github/overview).

To add a new I2C address, go to the markdown page associated with the
first hex digit.  For example, if you want to add a new device with
an I2C address of `0x4E`, you would go add it to the `0x40-0x4F.md`
page.

Within that page, find the header for the I2C Address you'd like to
add to.  Again, using `0x4E` as an example address, you would find
the header for it towards the end of the page.

You should then add it as a list item containing the device name and
the I2C address range(s) the device can take. For example, the `0x4E`
device (say, the ABC1234 temperature sensor) would be added as follows:

```markdown
- ABC1234 Temperature Sensor (0x4E)
```

If there is a product page associated with the device, you can add the link by using `[text](link)` notation:

```markdown
- [ABC1234 Temperature Sensor](wwww.link_to_product_page.com)
```

> **_NOTE:_** Make sure to add the device under all the headers it can
> use.  For example, if a device can use `0x35`, `0x36`, and `0x61`,
> you should add it under the `0x35` and `0x36` headers in `0x30-0x3F.md`,
> and under the `0x61` header in `0x60-0x6F.md`.
