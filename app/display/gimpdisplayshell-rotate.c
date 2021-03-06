/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "core/gimp-utils.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-expose.h"
#include "gimpdisplayshell-rotate.h"
#include "gimpdisplayshell-scale.h"


/*  public functions  */

void
gimp_display_shell_rotate (GimpDisplayShell *shell,
                           gdouble           delta)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimp_display_shell_rotate_to (shell, shell->rotate_angle + delta);
}

void
gimp_display_shell_rotate_to (GimpDisplayShell *shell,
                              gdouble           value)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  while (value < 0.0)
    value += 360;

  while (value >= 360.0)
    value -= 360;

  shell->rotate_angle = value;

  gimp_display_shell_rotated (shell);
  gimp_display_shell_expose_full (shell);
}

void
gimp_display_shell_rotate_drag (GimpDisplayShell *shell,
                                gdouble           last_x,
                                gdouble           last_y,
                                gdouble           cur_x,
                                gdouble           cur_y,
                                gboolean          constrain)
{
  gint    image_width, image_height;
  gdouble px, py;
  gdouble x1, y1, x2, y2;
  gdouble angle1, angle2, angle;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimp_display_shell_scale_get_image_size (shell,
                                           &image_width, &image_height);

  px = -shell->offset_x + image_width  / 2;
  py = -shell->offset_y + image_height / 2;

  x1 = cur_x  - px;
  x2 = last_x - px;
  y1 = py - cur_y;
  y2 = py - last_y;

  /*  find the first angle  */
  angle1 = atan2 (y1, x1);

  /*  find the angle  */
  angle2 = atan2 (y2, x2);

  angle = angle2 - angle1;

  if (angle > G_PI || angle < -G_PI)
    angle = angle2 - ((angle1 < 0) ? 2.0 * G_PI + angle1 : angle1 - 2.0 * G_PI);

  shell->rotate_drag_angle += (angle * 180.0 / G_PI);

  if (shell->rotate_drag_angle < 0.0)
    shell->rotate_drag_angle += 360;

  if (shell->rotate_drag_angle >= 360.0)
    shell->rotate_drag_angle -= 360;

  gimp_display_shell_rotate_to (shell,
                                constrain ?
                                (gint) shell->rotate_drag_angle / 15 * 15 :
                                shell->rotate_drag_angle);
}

void
gimp_display_shell_rotate_update_transform (GimpDisplayShell *shell)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  g_free (shell->rotate_transform);
  g_free (shell->rotate_untransform);

  if (shell->rotate_angle != 0.0 && gimp_display_get_image (shell->display))
    {
      gint    image_width, image_height;
      gdouble cx, cy;

      shell->rotate_transform   = g_new (cairo_matrix_t, 1);
      shell->rotate_untransform = g_new (cairo_matrix_t, 1);

      gimp_display_shell_scale_get_image_size (shell,
                                               &image_width, &image_height);

      cx = -shell->offset_x + image_width  / 2;
      cy = -shell->offset_y + image_height / 2;

      cairo_matrix_init_translate (shell->rotate_transform, cx, cy);
      cairo_matrix_rotate (shell->rotate_transform,
                           shell->rotate_angle / 180.0 * G_PI);
      cairo_matrix_translate (shell->rotate_transform, -cx, -cy);

      *shell->rotate_untransform = *shell->rotate_transform;
      cairo_matrix_invert (shell->rotate_untransform);
    }
  else
    {
      shell->rotate_transform   = NULL;
      shell->rotate_untransform = NULL;
    }
}

void
gimp_display_shell_rotate_transform_bounds (GimpDisplayShell *shell,
                                            gdouble           x1,
                                            gdouble           y1,
                                            gdouble           x2,
                                            gdouble           y2,
                                            gdouble          *nx1,
                                            gdouble          *ny1,
                                            gdouble          *nx2,
                                            gdouble          *ny2)
{
  gdouble tx1, ty1;
  gdouble tx2, ty2;
  gdouble tx3, ty3;
  gdouble tx4, ty4;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  tx1 = x1;
  ty1 = y1;
  tx2 = x1;
  ty2 = y2;
  tx3 = x2;
  ty3 = y1;
  tx4 = x2;
  ty4 = y2;

  cairo_matrix_transform_point (shell->rotate_transform, &tx1, &ty1);
  cairo_matrix_transform_point (shell->rotate_transform, &tx2, &ty2);
  cairo_matrix_transform_point (shell->rotate_transform, &tx3, &ty3);
  cairo_matrix_transform_point (shell->rotate_transform, &tx4, &ty4);

  *nx1 = MIN4 (tx1, tx2, tx3, tx4);
  *ny1 = MIN4 (ty1, ty2, ty3, ty4);
  *nx2 = MAX4 (tx1, tx2, tx3, tx4);
  *ny2 = MAX4 (ty1, ty2, ty3, ty4);
}

void
gimp_display_shell_rotate_untransform_bounds (GimpDisplayShell *shell,
                                              gdouble           x1,
                                              gdouble           y1,
                                              gdouble           x2,
                                              gdouble           y2,
                                              gdouble          *nx1,
                                              gdouble          *ny1,
                                              gdouble          *nx2,
                                              gdouble          *ny2)
{
  gdouble tx1, ty1;
  gdouble tx2, ty2;
  gdouble tx3, ty3;
  gdouble tx4, ty4;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  tx1 = x1;
  ty1 = y1;
  tx2 = x1;
  ty2 = y2;
  tx3 = x2;
  ty3 = y1;
  tx4 = x2;
  ty4 = y2;

  cairo_matrix_transform_point (shell->rotate_untransform, &tx1, &ty1);
  cairo_matrix_transform_point (shell->rotate_untransform, &tx2, &ty2);
  cairo_matrix_transform_point (shell->rotate_untransform, &tx3, &ty3);
  cairo_matrix_transform_point (shell->rotate_untransform, &tx4, &ty4);

  *nx1 = MIN4 (tx1, tx2, tx3, tx4);
  *ny1 = MIN4 (ty1, ty2, ty3, ty4);
  *nx2 = MAX4 (tx1, tx2, tx3, tx4);
  *ny2 = MAX4 (ty1, ty2, ty3, ty4);
}
