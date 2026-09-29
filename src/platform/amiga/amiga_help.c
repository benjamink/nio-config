#include "amiga_help.h"

#include <string.h>

static const char *const titles[AMIGA_HELP_TOPICS] = {
  "Contents",
  "Getting started",
  "Hosts",
  "Browsing and mounting",
  "Drives and ejecting",
  "Catalogue",
  "Settings",
  "Keyboard",
  "Starting automatically",
  "Options and ToolTypes",
  "Troubleshooting",
  "About"
};

static const char *const texts[AMIGA_HELP_TOPICS] = {
  /* Contents: the GUI lists the other titles below this text. */
  "Double-click a topic, or select it and press Return. Use Previous and "
  "Next to read the topics in order, and Close or Esc to go back.",

  "FujiNet Config lets you choose disk images on your FujiNet hosts and "
  "mount them as Amiga drives.\n"
  "\n"
  "1. On the Hosts page, double-click a host.\n"
  "2. On the Browse page, open drawers until you find the disk image, then "
  "double-click it.\n"
  "3. Choose a drive in the list. Read-only (RO) is ticked; untick it if "
  "you need to write to the disk. Press Mount.\n"
  "\n"
  "The disk appears on Workbench like any other disk. Every change is "
  "saved on the FujiNet at once, so there is nothing to save when you "
  "quit.",

  "The Hosts page lists the servers and devices FujiNet can browse, for "
  "example tnfs://fujinet.online or sd0:/ for the FujiNet's own SD card. "
  "Up to 8 hosts are kept.\n"
  "\n"
  "- Browse opens the selected host (double-click does the same).\n"
  "- Add adds the text in the URI field as a new host.\n"
  "- Replace changes the selected host to the text in the URI field.\n"
  "- Remove deletes the selected host after asking.\n"
  "- Move Up and Move Down change the order.\n"
  "\n"
  "A host without a scheme, such as fujinet.online, is treated as a TNFS "
  "server.",

  "The Browse page shows the drawers and files of the host you opened. "
  "Drawers are marked Drawer; files show their size and date.\n"
  "\n"
  "- Double-click a drawer, or press Open, to enter it.\n"
  "- Parent, or Backspace, goes up one level.\n"
  "- Refresh reads the listing again.\n"
  "- Double-click a disk image, or select it and press Mount..., to mount "
  "it.\n"
  "\n"
  "Mount lists the drives and what each one holds. The first empty drive "
  "is selected and RO is ticked. Choose a drive and press Mount. If the "
  "drive already holds a disk the button reads Replace and asks first. "
  "Cancel or Esc goes back without mounting.\n"
  "\n"
  "Very large drawers show their first 200 entries.",

  "The Drives page shows every FujiNet drive with the image mounted in it "
  "and whether it is read-only (RO) or read/write (RW).\n"
  "\n"
  "- Eject unmounts the selected drive after asking.\n"
  "\n"
  "On Workbench 2.0 and later the drives are DN0: to DN7:. On Workbench 1.3 "
  "they are DN0: and DN1: (double density, FFS), HN0: and HN1: (high "
  "density, FFS), DO0: and DO1: (double density, OFS) and HO0: and HO1: "
  "(high density, OFS).\n"
  "\n"
  "Mounted drives come back after a reboot if you run FMOUNTRESTORE.",

  "Project > Catalogue... shows the FujiNet slot catalogue: numbered slots "
  "0 to 255 that remember disk images. Mount fills these slots for you, "
  "so you normally never need this page.\n"
  "\n"
  "- Selecting a slot copies it into the URI, Slot and RO fields.\n"
  "- Set stores the URI field in the slot number given.\n"
  "- Clear empties the selected slot after asking.\n"
  "- Mount... mounts the slot on a drive you choose.\n"
  "\n"
  "Typing a number in Slot and pressing Return jumps to that slot.",

  "The Settings menu changes how the Browse page shows files. Settings "
  "are saved on the FujiNet at once.\n"
  "\n"
  "- Dates YY-MM-DD or YY-DD-MM chooses the date order.\n"
  "- Sizes Full shows exact byte counts; Sizes Compact shows Kb, Mb and "
  "Gb.\n"
  "\n"
  "Colours follow your Workbench palette.",

  "- Cursor Up and Down move the selection. With Shift they move a page; "
  "with Alt they go to the top or bottom.\n"
  "- Return, or a double-click, runs the page's main action.\n"
  "- Backspace goes to the parent drawer on the Browse page.\n"
  "- Tab and Shift-Tab change page.\n"
  "- Help opens this help. Esc closes help, cancels Mount, or quits.\n"
  "- Right-Amiga-C opens the catalogue, Right-Amiga-? shows About and "
  "Right-Amiga-Q quits.",

  "To start FujiNet Config every time Workbench starts, run the installer "
  "from the NIO package in a Shell:\n"
  "\n"
  "  Execute NIO:Install-config-nio\n"
  "\n"
  "It copies the program to SYS:Tools, installs the FujiNet commands and "
  "devices if they are missing, and asks whether to add FujiNet Config to "
  "SYS:WBStartup. Use AUTO or NOAUTO to answer in advance, and REMOVE to "
  "stop it starting automatically.\n"
  "\n"
  "Workbench 1.3 has no WBStartup drawer; the ReadMe explains the one line "
  "to add to S:Startup-Sequence.",

  "FujiNet Config reads KEY=value options from its icon's ToolTypes (use "
  "Information in the Workbench Icons menu) or from the Shell:\n"
  "\n"
  "- FMOUNT=path is the mount command (default SYS:C/fmount).\n"
  "- FUMOUNT=path is the eject command (default SYS:C/fumount).\n"
  "- SCRIPT=file runs commands from a file instead of the window.\n"
  "- RESULT=file is where SCRIPT writes its results.\n"
  "\n"
  "ToolTypes in brackets, such as (FMOUNT=...), are ignored.",

  "- \"FujiNet init failed\": fujinet-nio.device is not installed or not "
  "loaded. Run the installer and reboot.\n"
  "- \"FMOUNT failed ... Unknown command\": FMOUNT is not in SYS:C. Run "
  "the installer, or set the FMOUNT ToolType.\n"
  "- \"Cannot open fujinet-disk.device\": the disk device is not loaded. "
  "The installer adds it to S:User-Startup; reboot afterwards.\n"
  "- \"Browse failed\": the host could not be reached. Check the URI and "
  "the FujiNet's network.\n"
  "- \"FUMOUNT failed ... handler (busy)\": a program still uses the "
  "disk, for example an open drawer or a Shell whose current directory is "
  "on it. Close it and try again. The Workbench 3.1 filesystem cannot "
  "release a disk while it runs, so there the disk stays mounted until "
  "the next reboot.",

  "FujiNet Config for the Amiga, part of FujiNet NIO.\n"
  "\n"
  "It runs on Workbench 1.3 and later and uses the FMOUNT and FUMOUNT "
  "commands to mount FujiNet disk images."
};

const char *amiga_help_title(uint8_t topic)
{
  return titles[topic < AMIGA_HELP_TOPICS ? topic : AMIGA_HELP_CONTENTS];
}

const char *amiga_help_text(uint8_t topic)
{
  return texts[topic < AMIGA_HELP_TOPICS ? topic : AMIGA_HELP_CONTENTS];
}

uint8_t amiga_help_find(const char *title)
{
  uint8_t t;

  for (t = 0; t < AMIGA_HELP_TOPICS; t++) {
    if (strcmp(titles[t], title) == 0)
      return t;
  }
  return AMIGA_HELP_CONTENTS;
}

uint16_t amiga_help_layout(const char *text, uint8_t cols,
                           amiga_help_line_t *lines, uint16_t max)
{
  uint16_t n = 0;
  uint16_t p = 0;

  if (!text || cols < 8)
    return 0;
  while (n < max) {
    uint16_t end = p;
    uint8_t hang;
    uint8_t first = 1;

    while (text[end] && text[end] != '\n')
      end++;
    hang = (uint8_t) (end - p >= 2 && text[p] == '-' && text[p + 1] == ' '
                      ? 2 : 0);
    if (end == p) {
      lines[n].start = p;
      lines[n].len = 0;
      lines[n].indent = 0;
      n++;
    }
    while (p < end && n < max) {
      uint8_t indent = (uint8_t) (first ? 0 : hang);
      uint16_t width = (uint16_t) (cols - indent);
      uint16_t take;

      if (!first)
        while (p < end && text[p] == ' ')
          p++;
      if (p >= end)
        break;
      if ((uint16_t) (end - p) <= width) {
        take = (uint16_t) (end - p);
      } else if (text[p + width] == ' ') {
        take = width;
      } else {
        take = width;
        while (take > 0 && text[p + take - 1] != ' ')
          take--;
        if (take == 0)
          take = width;                   /* word longer than the line */
        else
          while (take > 0 && text[p + take - 1] == ' ')
            take--;
      }
      lines[n].start = p;
      lines[n].len = (uint8_t) take;
      lines[n].indent = indent;
      n++;
      p = (uint16_t) (p + take);
      first = 0;
    }
    if (!text[end])
      break;
    p = (uint16_t) (end + 1);
  }
  return n;
}
