#!/usr/bin/env python3
"""Generate demo assets for the Paperman app.

Produces three PDFs and a per-document thumbnail for each one.
Requires: reportlab, Pillow, pdftoppm (poppler-utils).

Usage:
    python3 app/tools/gen_demo_assets.py
"""

import os
import random
import shutil
import subprocess
import sys
import tempfile

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import inch
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas
from reportlab.platypus import (
    SimpleDocTemplate, PageBreak, Paragraph, Spacer, Table, TableStyle,
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEMO_DIR = os.path.join(SCRIPT_DIR, '..', 'assets', 'demo')

# Import make_plasma_jpeg from sibling script
sys.path.insert(0, os.path.join(SCRIPT_DIR, '..', '..', 'scripts'))
from make_test_files import make_plasma_jpeg

W, H = A4

LOREM_WORDS = (
    'lorem ipsum dolor sit amet consectetur adipiscing elit sed do '
    'eiusmod tempor incididunt ut labore et dolore magna aliqua ut enim '
    'ad minim veniam quis nostrud exercitation ullamco laboris nisi ut '
    'aliquip ex ea commodo consequat duis aute irure dolor in '
    'reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla '
    'pariatur excepteur sint occaecat cupidatat non proident sunt in '
    'culpa qui officia deserunt mollit anim id est laborum'
).split()


def lorem_paragraph(rng, min_words=40, max_words=80):
    n = rng.randint(min_words, max_words)
    words = [rng.choice(LOREM_WORDS) for _ in range(n)]
    words[0] = words[0].capitalize()
    return ' '.join(words) + '.'


def gen_sample_pdf(path):
    """Five-page colour PDF with text and tables."""
    styles = getSampleStyleSheet()
    title_style = ParagraphStyle(
        'DemoTitle', parent=styles['Title'],
        textColor=colors.HexColor('#1a5276'),
    )
    heading_style = ParagraphStyle(
        'DemoHeading', parent=styles['Heading2'],
        textColor=colors.HexColor('#196f3d'),
    )
    body_style = styles['BodyText']

    doc = SimpleDocTemplate(path, pagesize=A4)
    story = []
    rng = random.Random(99)

    # Page 1: title and intro
    story.append(Paragraph('Quarterly Status Report', title_style))
    story.append(Spacer(1, 12))
    story.append(Paragraph(
        'This document demonstrates the Paperman document viewer. '
        'It contains formatted text, tables and colour to exercise '
        'the rendering pipeline.',
        body_style,
    ))
    story.append(Spacer(1, 12))
    story.append(Paragraph(lorem_paragraph(rng), body_style))
    story.append(Spacer(1, 24))

    # Summary table
    story.append(Paragraph('Project Summary', heading_style))
    story.append(Spacer(1, 8))
    summary_data = [
        ['Metric', 'Q1', 'Q2', 'Q3', 'Q4'],
        ['Revenue', '12,400', '15,800', '14,200', '18,600'],
        ['Users', '1,240', '1,580', '1,710', '2,030'],
        ['Uptime %', '99.2', '99.7', '99.5', '99.9'],
        ['Tickets', '84', '62', '71', '45'],
    ]
    t = Table(summary_data, colWidths=[1.5 * inch] + [inch] * 4)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#1a5276')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, -1), 10),
        ('ALIGN', (1, 0), (-1, -1), 'RIGHT'),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1),
         [colors.HexColor('#eaf2f8'), colors.white]),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#aab7b8')),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
    ]))
    story.append(t)

    # Pages 2-5: section with text and a small table each
    sections = [
        ('Infrastructure', ['Servers', 'Storage', 'Network', 'Backups']),
        ('Development', ['Features', 'Bug fixes', 'Reviews', 'Deploys']),
        ('Support', ['Tickets', 'Resolved', 'Escalated', 'Avg hours']),
        ('Planning', ['Sprints', 'Completed', 'Carried', 'Velocity']),
    ]
    for title, cols in sections:
        story.append(PageBreak())
        story.append(Paragraph(title, heading_style))
        story.append(Spacer(1, 8))
        story.append(Paragraph(lorem_paragraph(rng, 60, 100), body_style))
        story.append(Spacer(1, 8))
        story.append(Paragraph(lorem_paragraph(rng, 40, 70), body_style))
        story.append(Spacer(1, 8))
        story.append(Paragraph(lorem_paragraph(rng, 50, 80), body_style))
        story.append(Spacer(1, 12))

        rows = [cols]
        for _ in range(4):
            rows.append([str(rng.randint(10, 999)) for _ in cols])
        t = Table(rows, colWidths=[1.2 * inch] * len(cols))
        t.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#196f3d')),
            ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
            ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
            ('FONTSIZE', (0, 0), (-1, -1), 9),
            ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
            ('ROWBACKGROUNDS', (0, 1), (-1, -1),
             [colors.HexColor('#eafaf1'), colors.white]),
            ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#aab7b8')),
            ('TOPPADDING', (0, 0), (-1, -1), 3),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
        ]))
        story.append(t)

    doc.build(story)
    print(f'  {os.path.basename(path)}')


def gen_image_doc_pdf(path):
    """Two-page PDF: embedded image then text."""
    c = canvas.Canvas(path, pagesize=A4)

    plasma_path = os.path.join(os.path.dirname(path), 'plasma.jpg')
    make_plasma_jpeg(plasma_path, width=480, height=480, quality=80)

    c.setFont('Helvetica-Bold', 16)
    c.drawString(72, H - 72, 'Embedded Image')
    img = ImageReader(plasma_path)
    img_w = W - 144
    c.drawImage(img, 72, H - 100 - img_w, width=img_w, height=img_w,
                preserveAspectRatio=True)
    c.showPage()

    c.setFont('Helvetica-Bold', 24)
    c.drawString(72, H - 100, 'Sample Image Document')
    c.setFont('Helvetica', 12)
    text = c.beginText(72, H - 140)
    for line in [
        'This is a demonstration document bundled with the Paperman app.',
        'It contains two pages to showcase the document viewer.',
        '',
        'The first page displays an embedded image to demonstrate that',
        'the viewer handles mixed content correctly. This second page',
        'shows formatted text content.',
        '',
        'Paperman is a document management application that allows you',
        'to browse, search, and view documents stored on a server.',
        'This demo mode lets you explore the app without needing a server.',
    ]:
        text.textLine(line)
    c.drawText(text)
    c.showPage()
    c.save()
    print(f'  {os.path.basename(path)}')


def gen_long_report_pdf(path):
    """100-page report with coloured headings."""
    heading_colours = [
        colors.HexColor('#1a5276'),
        colors.HexColor('#7b241c'),
        colors.HexColor('#196f3d'),
        colors.HexColor('#7d6608'),
        colors.HexColor('#6c3483'),
        colors.HexColor('#0e6655'),
        colors.HexColor('#a04000'),
        colors.HexColor('#2e4053'),
    ]
    rng = random.Random(42)

    c = canvas.Canvas(path, pagesize=A4)
    for page_num in range(1, 101):
        colour = heading_colours[page_num % len(heading_colours)]
        c.setFillColor(colour)
        c.setFont('Helvetica-Bold', 18)
        c.drawString(72, H - 72, f'Chapter {page_num}')
        c.setStrokeColor(colour)
        c.setLineWidth(1.5)
        c.line(72, H - 78, W - 72, H - 78)

        c.setFillColor(colors.HexColor('#333333'))
        text = c.beginText(72, H - 110)
        text.setFont('Helvetica', 11)
        for _ in range(rng.randint(3, 5)):
            para = lorem_paragraph(rng)
            line = ''
            for word in para.split():
                if len(line) + len(word) + 1 > 80:
                    text.textLine(line)
                    line = word
                else:
                    line = f'{line} {word}'.strip()
            if line:
                text.textLine(line)
            text.textLine('')
        c.drawText(text)
        c.showPage()
    c.save()
    print(f'  {os.path.basename(path)}')


def gen_thumbnail(pdf_path, thumb_path):
    """Render first page of a PDF to a 96px-wide JPEG thumbnail."""
    with tempfile.TemporaryDirectory() as tmp:
        prefix = os.path.join(tmp, 'thumb')
        subprocess.run(
            ['pdftoppm', '-jpeg', '-f', '1', '-l', '1',
             '-scale-to', '96', pdf_path, prefix],
            check=True,
        )
        # pdftoppm appends -1 or -01 or -001 depending on page count
        for f in sorted(os.listdir(tmp)):
            if f.endswith('.jpg'):
                shutil.move(os.path.join(tmp, f), thumb_path)
                print(f'  {os.path.basename(thumb_path)}')
                return
    raise RuntimeError(f'pdftoppm produced no output for {pdf_path}')


def main():
    os.makedirs(DEMO_DIR, exist_ok=True)

    print('Generating demo PDFs...')
    sample = os.path.join(DEMO_DIR, 'sample.pdf')
    image_doc = os.path.join(DEMO_DIR, 'image_doc.pdf')
    long_report = os.path.join(DEMO_DIR, 'long_report.pdf')

    gen_sample_pdf(sample)
    gen_image_doc_pdf(image_doc)
    gen_long_report_pdf(long_report)

    print('Generating thumbnails...')
    gen_thumbnail(sample, os.path.join(DEMO_DIR, 'thumb_summary.jpg'))
    gen_thumbnail(image_doc, os.path.join(DEMO_DIR, 'thumb_image.jpg'))
    gen_thumbnail(long_report, os.path.join(DEMO_DIR, 'thumb_report.jpg'))

    print('Done.')


if __name__ == '__main__':
    main()
