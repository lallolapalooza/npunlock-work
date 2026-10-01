/* minimal repro: kernel does not define the requested entry symbol */
void not_the_entry(unsigned layerParams) {
  (void)layerParams;
}
