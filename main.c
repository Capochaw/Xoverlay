#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void rgba_to_bgra(unsigned char *data, int w, int h) {
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = data + i * 4;

        unsigned char tmp = p[0]; // R
        p[0] = p[2];             // B
        p[2] = tmp;              // R
    }
}

int main(int argc,char* argv[]) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;

    if(argc == 1){
        printf("ERROR: image path not loaded.\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    // Región activa
    unsigned char *img = NULL;
    int w = 0, h = 0, ch = 0;
    while(img == NULL){
        img = stbi_load(argv[1], &w, &h, &ch, 4);
        if (!img) {
            printf("Error recargando imagen\n");
            continue;
        }
        rgba_to_bgra(img, w, h);
    }
    int rx = 1414, ry = 27, rw = 300, rh = 300;
    int vrx = 1620, vry = 0, vrw = 300, vrh = 22;

    // Visual con alpha
    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
        printf("No ARGB visual\n");
        return 1;
    }

    XSetWindowAttributes attr;
    attr.override_redirect = True;
    attr.colormap = XCreateColormap(dpy, root, vinfo.visual, AllocNone);
    attr.background_pixel = 0;
    attr.border_pixel = 0;

    // Ventana overlay
    Window win = XCreateWindow(
        dpy, root,
        rx, ry, w,h,
        0,
        vinfo.depth,
        InputOutput,
        vinfo.visual,
        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel,
        &attr
    );

    GC gc = XCreateGC(dpy, win, 0, NULL);

    XImage *ximg = NULL;

    int visible = 1;

    // timer recarga
    time_t last_reload = 0;
    int reload_interval = 1;


    while (1) {
        // ===== detectar mouse =====
        Window ret_root, ret_child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;

        XQueryPointer(dpy, root, &ret_root, &ret_child,
                      &root_x, &root_y,
                      &win_x, &win_y, &mask);

        int inside = (root_x >= vrx && root_x <= vrx+vrw &&
                      root_y >= vry && root_y <= vry+vrh);

        // ===== recargar imagen =====
        time_t now = time(NULL);

        if (now - last_reload >= reload_interval && visible) {
            last_reload = now;

            // destruir imagen anterior correctamente
            if (ximg) {
                XDestroyImage(ximg); // libera también data
                ximg = NULL;
                img = NULL;
            }

            img = stbi_load(argv[1], &w, &h, &ch, 4);
            if (!img) {
                printf("Error recargando imagen\n");
                continue;
            }
            rgba_to_bgra(img, w, h);

            ximg = XCreateImage(
                dpy,
                vinfo.visual,
                32,
                ZPixmap,
                0,
                (char*)img,
                w, h,
                32,
                0
            );

            // ajustar tamaño de ventana a la imagen
            XResizeWindow(dpy, win, w, h);

            if (visible) {
                XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, w, h);
            }
        }

        // ===== mostrar / ocultar =====
        if (inside && !visible && ximg) {
            XMapRaised(dpy, win);
            XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, w, h);
            visible = 1;
        } else if (!inside && visible) {
            XUnmapWindow(dpy, win);
            visible = 0;
        }

        usleep(10000);
    }

    return 0;
}
