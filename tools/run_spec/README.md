1. Setup spec2017 which copies spec2017 to $HOME

```
make setup
```

2. Setup the environment

```
source envsetup.sh [ clang2mpl | mplfe ]
```

3. Build and test
   - For single benchmark 605.mcf_s:

```
make build.605
make test.605
make train.605
make ref.605
```

