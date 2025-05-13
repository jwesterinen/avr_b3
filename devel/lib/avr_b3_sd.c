/*
 *  avr_b3_sd.c
 */

// globals required for SD file system
FIL       fp;
FATFS     diskA;
DIR       topdir;
FRESULT   retstat;

char fileBuffer[MAX_PROGRAM_LEN][MAX_CMDLINE_LEN];

bool SdMount(void)
{
    // initialize SD card, mount FAT filesystem, and open root directory
    retstat = disk_initialize(0);
    if (retstat != FR_OK) 
    {
        sprintf(errorStr, "mount failed (error %d)\n", retstat);
        return false;
    }
    retstat = f_mount(&diskA, "/", 0);
    if (retstat != FR_OK) 
    {
        sprintf(errorStr, "mount failed (error %d)\n", retstat);
        return false;
    }
    retstat = f_opendir(&topdir, "");
    if (retstat != FR_OK) 
    {
        sprintf(errorStr, "mount failed (error %d)\n", retstat);
        return false;
    }

    return true;
}

bool SdUnmount(void)
{
    retstat = f_unmount("/");
    if (retstat != FR_OK)
    {
        sprintf(errorStr, "unmount failed (err %d)\n", retstat);
        return false;
    }
    PutString("...it is safe to remove the disk\n");

    return true;
}

bool SdList(void)
{
    FILINFO fno;
    
    retstat = f_readdir(&topdir, &fno);
    while ((retstat == FR_OK) && (fno.fname[0])) 
    {
        fno.fname[12] = 0;
        sprintf(message, "%s\n", fno.fname);
        PutString(message);
        retstat = f_readdir(&topdir, &fno);
    }
    if (retstat != FR_OK)
    {
        sprintf(errorStr, "file listing failed (err %d)\n", retstat);
        return false;
    }
    retstat = f_rewinddir(&topdir);
    if (retstat != FR_OK)
    {
        sprintf(errorStr, "file listing failed (err %d)\n", retstat);
        return false;
    }

    return true;
}

bool SdDelete(const char *filename)
{
    if (filename != NULL)
    {
        retstat = f_unlink(filename);
        if (retstat != FR_OK)
        {
            sprintf(errorStr, "file delete failed (err=%d)\n", retstat);
            return false;
        }
    }
    else
    {
        strcpy(errorStr, "missing filename");
        return false;
    }

    return true;
}

bool SdLoad(const char *filename)
{
    TCHAR *bufptr;
    int i;
    char *nextChar;
    
    if (filename != NULL)
    {
        // open the file
        retstat = f_open(&fp, filename, FA_READ);
        if (retstat != FR_OK) 
        {
            sprintf(errorStr, "load failed (err %d)\n", retstat);
            return false;
        }

        // read the file (until EOF) into the file buffer
        for (i = 0; i < MAX_PROGRAM_LEN; i++) 
        {
            if (f_eof(&fp) != 0)
            {
                // stop reading at EOF
                break;
            }
            bufptr = f_gets(fileBuffer[i], MAX_CMDLINE_LEN, &fp);
            if (bufptr == 0) 
            {
                sprintf(errorStr, "load failed (err %d)\n", f_error(&fp));
                return false;
            }
        }
        strcpy(fileBuffer[i], "");

        // close the file
        retstat = f_close(&fp);
        if (retstat != FR_OK) 
        {
            sprintf(errorStr, "load failed (err %d)\n", retstat);
            return false;
        }
        
        // process each line of the file buffer
        for (i = 0; fileBuffer[i][0] != '\0'; i++)
        {
            // remove any line endings
            for (nextChar = fileBuffer[i]; *nextChar != '\0'; nextChar++)
            {
                if (*nextChar == '\n' || *nextChar == '\r')
                {
                    *nextChar = '\0';
                    break;
                }
            }
            
            if (!ProcessCommand(fileBuffer[i]))
            {
                PutString(errorStr);
                PutString("\n");
            }
        }
    }
    else
    {
        strcpy(errorStr, "missing filename");
        return false;
    }

    return true;
}

// save the file buffer to a new file
bool SdSave(const char *filename)
{
    int i;
    
    if (filename != NULL)
    {
        // create a new file
        retstat = f_open(&fp, filename, (FA_CREATE_ALWAYS | FA_READ | FA_WRITE));
        if (retstat != FR_OK) 
        {
            sprintf(errorStr, "save failed (err %d)\n", retstat);
            return false;
        }

        // fill the file buffer with the program commands
        for (i = 0; i < programSize; i++)
        {
            strcpy(fileBuffer[i], Program[i].commandStr);
            strcat(fileBuffer[i], "\n");
        }
        strcpy(fileBuffer[i], "");
        
        // write the buffer contents to the currently open file
        for (int line = 0; fileBuffer[line][0] != '\0'; line++)
        {
            if (f_puts(fileBuffer[line], &fp) < 0) 
            {
                sprintf(errorStr, "save failed at line %d\n", line);
                return false;
            }
        }

        // flushing data to disk
        retstat = f_sync(&fp);
        if (retstat != FR_OK) 
        {
            sprintf(errorStr, "save failed (err %d)\n", retstat);
            return false;
        }

        // close the currently open file
        retstat = f_close(&fp);
        if (retstat != FR_OK) 
        {
            sprintf(errorStr, "save failed (err %d)\n", retstat);
            return false;
        }
    }
    else
    {
        strcpy(errorStr, "missing filename");
        return false;
    }

    return true;
}


