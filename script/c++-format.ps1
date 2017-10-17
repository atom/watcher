$srcfiles = Get-ChildItem -Path src -Filter *.cpp -Recurse | %{$_.FullName}

clang-format -style=file -i $srcfiles
