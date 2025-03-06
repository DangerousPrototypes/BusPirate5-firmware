![image|644x342](upload://9bifspiic1OMP7m8hNNKXjbO747.png)

The I2C sniffer is ready to merge into main. 

I2C sniffing is provided via the [pico-i2c-sniff](https://github.com/jjsch-dev/pico_i2c_sniffer) project. It uses all four state machines of a PIO to accuratly sniff the traffic on an I2C bus at up to 500kHz.

## Connections

- Connect SDA (IO0) to the I2C data pin of the bus to sniff
- Connect SCL (IO1) to the I2C clock pin of the bus to sniff

## Setup

Access the I2C sniffer from I2C mode:
- ```m``` - Select mode
- ```5``` - Choose ```I2C``` mode

The speed and clock stretch settings have no effect on the sniffer speed. Choose the defaults by pressing ```enter```.

:::tip
Sniffer speed is independent from the I2C mode transmit speed. The sniffer will work up to 500kHz regardless of the I2C speed selected during configuration. 
:::

## Use

[image]

To begin sniffing type the ```sniff``` command followed by ```enter```.

### Output Format

```
I2C> [ 0 1 2 3 4]

I2C START
TX: 0 NACK 1 NACK 2 NACK 3 NACK 4 NACK
I2C STOP
I2C>
```
We'll send this very simple I2C packet using one Bus Pirate, and sniff it with a second Bus Pirate.
- ```[``` - I2C START bit
- ```0 1 2 3 4``` - Four bytes to transmit
- ```]``` - I2C STOP bit

No actual I2C device is connected, so the bytes with not be acknowledged (ACKed).

> [ 0x00- 0x01- 0x02- 0x03- 0x04-]

I2C sniffer results use a similar syntax so they can be replayed by pasting them into the I2C mode command line.  
- ```[``` and ```]``` - I2C START/STOP
- ```0x00``` - A byte of data sniffed
- ```+``` and ```-``` - Indicates I2C ACK/NAK for a data byte

Here all the bytes are NAKed because there's no actual device connected.

## Options

Type ```sniff -h``` to see the most recent options and usage info.
- ```q``` - Don't display ACKs/+ (success) so it is easier to replay sniffed packets by pasting into the I2C command line. NAKs/- (fail or end of read) will still be displayed.

## Good to Know

In sniffer mode pins IO2 and IO3 are used internally to track the I2C bus state. They will be labeled EV0 and EV1, do not make any external connections to these pins.


:::warning
Do not connect to IO2 (EV0) or IO3 (EV1) in I2C sniffer mode.
:::