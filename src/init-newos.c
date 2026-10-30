/* NEWOS OpenRC init - reemplaza /init (nsh). Sin fork/exec; syscall entry. */
#include <drivers/graphics/fb.h>
#include <sys/syscall_abi.h>
#include <core/core_printk.h>

int main(void) {
    char *v[] = {"/bin/voxel", NULL};
    char *t[] = {"/bin/tcc", "-nostdlib", "-run", "/etc/tcc-hello.c", NULL};
    char *n[] = {"/bin/nano", "/etc/tcc-hello.c", NULL};
    pr_info("openrc:newos gui=1 -> /bin/voxel (unico desktop)\n");
    sys_spawn2("/bin/voxel", v, 1);  /* desktop unico */
    sys_spawn2("/bin/tcc", t, 4);     /* demo */
    /* nano disponible; no auto-lanzar si no se pide */
    sys_sleep(2000);
    while (1) sys_sleep(1000);
    return 0;
}
