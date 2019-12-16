

// set USE_REAL_MALLOC to 0 to return mocked value
int USE_REAL_MALLOC = 1;
void * __real_malloc( size_t size );
void * __wrap_malloc( size_t size )
{
  if (USE_REAL_MALLOC == 1) {
    return __real_malloc( size );
  } else {
    return (void *) mock();
  }
}

// set USE_REAL_STRNDUP to 0 to return mocked value
int USE_REAL_STRNDUP = 1;
char* __real_strndup(const char *s, size_t n);
void* __wrap_strndup(const char *s, size_t n)
{
      if (USE_REAL_STRNDUP == 1) {
        return __real_strndup(s, n);
      } else {
         return (char *) mock();
      }
}

void reset_mocks() {
  USE_REAL_MALLOC = 1;
  USE_REAL_STRNDUP = 1;
}
