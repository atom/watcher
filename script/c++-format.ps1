$srcfiles = Get-ChildItem -Path src -Include *.cpp,*.h -Recurse | %{$_.FullName}

clang-format -style=file -i $srcfiles
