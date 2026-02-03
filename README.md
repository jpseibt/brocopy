# Brocopy
The idea of brocopy is to "broadcast" a file, copying it to paths defined in a **.csv** based on "keys" passed as arguments.

### Example .csv:
    key,path
    foo,C:\foo\bar\baz\bro.prn
    bar,\\123.12.1.12\Users\jose.seibt\Documents\foo\bro.prn
    baz,\\localhost\SharedPrinter

### Example call:
    broadcast_pjob.exe D:\foo\bar\baz\file.out D:\bar\paths.csv foo¹ bar² baz³
  
### Copies ```"D:\foo\bar\baz\file.out"``` to:
    1. "C:\foo\bar\baz\bro.prn"<br>
    2. "\\123.12.1.12\Users\jose.seibt\Documents\foo\bro.prn"<br>
    3. "\\localhost\SharedPrinter"<br>

The example program (broadcast_pjob.exe) aims to send a print job to a number of printers, using a **Mfilemon port** that is configured to 
create a file from the printer's driver output and pass its path to the program (argv[1]), the path to the CSV file that defines the 
**destination paths** (argv[2]), and one or more **keys** (argv[3]...) that will be matched against the first column of the given CSV.

- Mfilemon repo - https://github.com/lomo74/mfilemon
