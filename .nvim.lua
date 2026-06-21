print("loaded project configuration")
vim.cmd([[set makeprg=cmake\ -S\ .\ -B\ build\ &&\ cmake\ --build\ build -j]])
