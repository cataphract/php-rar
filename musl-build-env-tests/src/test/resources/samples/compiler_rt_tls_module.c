_Thread_local int dynamically_loaded_tls;

int touch_dynamically_loaded_tls(void)
{
    return ++dynamically_loaded_tls;
}
