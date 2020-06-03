# Source Code of "The Sixth Edition of Unix"
## How to get the source codes
All source codes are from [Index of /Archive/Distributions/Research/Dennis_v6](https://minnie.tuhs.org/Archive/Distributions/Research/Dennis_v6/) as follows.
```sh
# download source codes
wget https://minnie.tuhs.org/Archive/Distributions/Research/Dennis_v6/v6root.tar.gz
wget https://minnie.tuhs.org/Archive/Distributions/Research/Dennis_v6/v6src.tar.gz
# extract files
mkdir v6root
tar -C v6root -xzf v6root.tar.gz
tar -C v6root/usr/source -xzf v6src.tar.gz
# relocate source codes
mkdir sys source
cp -r v6root/usr/sys/ sys
cp -r v6root/usr/source/ source
# delete unnecessary files
rm -f v6root.tar.gz v6src.tar.gz
```



## Directory Structure
- v6root: root directory
- source: copied from v6root/usr/source
- sys: copied from v6root/usr/sys



## Links
- [The Unix Tree](https://minnie.tuhs.org/cgi-bin/utree.pl)
- [V6](https://minnie.tuhs.org/cgi-bin/utree.pl?file=V6)
- [Index of /Archive/Distributions/Research/Dennis_v6](https://minnie.tuhs.org/Archive/Distributions/Research/Dennis_v6/)
