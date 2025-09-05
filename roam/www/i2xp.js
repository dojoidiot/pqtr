/**
 * I2xP JavaScript Module - Refactored and Simplified
 * Clean, maintainable terminal with choice system
 */

// Horyzon Color Palette - Simplified
const HORYZON_COLORS = {
    DEEP_NAVY: 'rgb(15, 15, 71)',
    DENIM_BLUE: 'rgb(65, 90, 140)',
    CADET_BLUE: 'rgb(90, 135, 185)',
    TERRACOTTA_RED: 'rgb(194, 88, 75)',
    BURNT_ORANGE: 'rgb(210, 125, 65)',
    GOLDEN_OCHRE: 'rgb(224, 168, 58)',
    LIGHT_STEEL: 'rgb(220, 224, 230)',
    SLATE_GREY: 'rgb(150, 158, 170)',
    CHARCOAL_GREY: 'rgb(75, 80, 90)'
};

// --- ASCII Art Functions ---

function autoGlyf(containerId, textContent, sublineText) {
    const container = document.getElementById(containerId);
    if (!container) return;

    const canvas = document.createElement('canvas');
    container.appendChild(canvas);
    const ctx = canvas.getContext('2d');

    const shrug = '¯\\_(ツ)_/¯';
    const boxArt = createAsciiBox(textContent);
    const fullText = `${boxArt}\n${shrug}\n${sublineText}`;

    let fontSize = 50;
    let isScrambling = false;

    function sizeCanvas() {
        const dpr = window.devicePixelRatio || 1;
        const rect = container.getBoundingClientRect();
        if (rect.width === 0) return;

        const lines = fullText.split('\n');
        const longestLine = lines.reduce((a, b) => a.length > b.length ? a : b);

        ctx.font = `bold ${fontSize}px 'Roboto Mono', monospace`;
        const measuredWidth = ctx.measureText(longestLine).width;
        if (measuredWidth === 0) return;

        const scaleFactor = rect.width / measuredWidth;
        fontSize = fontSize * scaleFactor;

        const lineHeight = fontSize * 1.1;
        const sublineFontSize = fontSize * 0.6;
        const boxLines = boxArt.split('\n');
        const canvasHeight = (boxLines.length * lineHeight) + lineHeight + sublineFontSize * 1.1 + (fontSize * 0.4);

        canvas.style.width = `${rect.width}px`;
        canvas.style.height = `${canvasHeight}px`;
        canvas.width = rect.width * dpr;
        canvas.height = canvasHeight * dpr;
        ctx.scale(dpr, dpr);
    }

    function draw() {
        const containerRect = container.getBoundingClientRect();
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.textBaseline = 'middle';
        ctx.fillStyle = HORYZON_COLORS.LIGHT_STEEL;

        const boxLines = boxArt.split('\n');
        const lineHeight = fontSize * 1.1;

        // Draw box art
        ctx.font = `bold ${fontSize}px 'Roboto Mono', monospace`;
        ctx.textAlign = 'left';
        boxLines.forEach((line, lineIndex) => {
            const y = (lineHeight / 2) + (lineIndex * lineHeight);
            const textWidth = ctx.measureText(line).width;
            let currentX = (containerRect.width - textWidth) / 2;
            
            if (isScrambling && Math.random() > 0.95) {
                currentX += (Math.random() - 0.5) * 10;
            }
            
            for (const char of line) {
                ctx.fillText(char, currentX, y);
                currentX += ctx.measureText(char).width;
            }
        });

        // Draw shrug
        ctx.textAlign = 'center';
        const shrugY = (boxLines.length * lineHeight) + (lineHeight / 2);
        ctx.fillText(shrug, containerRect.width / 2, shrugY);

        // Draw subline
        const sublineFontSize = fontSize * 0.6;
        const sublineY = shrugY + (lineHeight / 2) + (sublineFontSize * 0.55) + (fontSize * 0.4);
        ctx.font = `${sublineFontSize}px 'Roboto Mono', monospace`;
        ctx.fillText(sublineText, containerRect.width / 2, sublineY);
    }

    function scrambleText() {
        const chars = ".:-=+*#%@/_><&!^/-[]";
        const scrambleChance = 0.2;
        isScrambling = true;
        
        setTimeout(() => { isScrambling = false; }, 250);
    }

    function renderLoop() {
        draw();
        requestAnimationFrame(renderLoop);
    }

    setInterval(() => {
        if (!document.hidden) scrambleText();
    }, 3000);

    window.addEventListener('resize', sizeCanvas);
    sizeCanvas();
    renderLoop();
}

function createAsciiBox(text) {
    const maxWidth = 20;
    const padding = 1;
    const lines = wordWrap(text, maxWidth);
    const longestLine = Math.max(...lines.map(line => line.length));
    const boxWidth = longestLine + padding * 2;
    
    const topBorder = '+' + '-'.repeat(boxWidth) + '+';
    const bottomBorder = '+' + '-'.repeat(boxWidth) + '+';
    const middleLines = lines.map(line => {
        const totalPadding = boxWidth - line.length;
        const leftPadding = Math.floor(totalPadding / 2);
        const rightPadding = Math.ceil(totalPadding / 2);
        return '|' + ' '.repeat(leftPadding) + line + ' '.repeat(rightPadding) + '|';
    }).join('\n');
    
    return `${topBorder}\n${middleLines}\n${bottomBorder}`;
}

function wordWrap(text, maxWidth) {
    const words = text.split(' ');
    const lines = [];
    let currentLine = '';
    
    for (const word of words) {
        if ((currentLine + ' ' + word).trim().length > maxWidth) {
            lines.push(currentLine.trim());
            currentLine = word;
        } else {
            currentLine = (currentLine + ' ' + word).trim();
        }
    }
    if (currentLine) lines.push(currentLine.trim());
    return lines;
}

// --- Background Static Effect ---

function backBuzz(canvasId) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    const colors = [
        [15, 15, 71], [65, 90, 140], [90, 135, 185],
        [194, 88, 75], [210, 125, 65], [224, 168, 58],
        [220, 224, 230], [150, 158, 170], [75, 80, 90]
    ];

    function resize() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    }

    function drawStatic() {
        if (canvas.width === 0 || canvas.height === 0) return;
        
        const imageData = ctx.createImageData(canvas.width, canvas.height);
        const data = imageData.data;
        
        for (let i = 0; i < data.length; i += 4) {
            const color = colors[Math.floor(Math.random() * colors.length)];
            const alpha = Math.random() * 0.3 + 0.1;
            
            data[i] = color[0];
            data[i + 1] = color[1];
            data[i + 2] = color[2];
            data[i + 3] = Math.floor(255 * alpha);
        }
        
        ctx.putImageData(imageData, 0, 0);
    }

    window.addEventListener('resize', resize);
    resize();
    setInterval(drawStatic, 100);
}

// --- Terminal Controller ---

function autoChat(canvas, options = {}) {
    if (!canvas || !(canvas instanceof HTMLCanvasElement)) {
        throw new Error('Valid canvas element required');
    }

    const ctx = canvas.getContext('2d');

    // Configuration
    const config = {
        fontSize: 16,
        lineHeight: 24,
        padding: 20,
        fontFamily: 'monospace',
        textColor: HORYZON_COLORS.LIGHT_STEEL,
        backgroundColor: HORYZON_COLORS.DEEP_NAVY,
        statusBarColor: HORYZON_COLORS.DENIM_BLUE,
        cursorChar: '█',
        cursorBlinkRate: 500,
        typingSpeed: 50,
        onChoiceSelected: null,
        ...options
    };

    // State
    let lines = [''];
    let maxLines = 0;
    let textQueue = [];
    let currentTypingText = null;
    let charIndex = 0;
    let lastTypeTime = 0;
    let cursorVisible = true;
    let lastBlinkTime = 0;
    let animationFrameId = null;
    let buttons = [];

    // --- Choice System ---
    
    class ChoiceRenderer {
        static parseChoice(text) {
            const pattern = /\(choice\s*\(name\s+(\w+)\)\s*\(\s*list\s*\(([^)]+)\)\s*\)\s*\)/g;
            const matches = [...text.matchAll(pattern)];
            
            return matches.map(match => ({
                fullMatch: match[0],
                name: match[1],
                options: match[2].split(',').map(opt => opt.trim())
            }));
        }

        static renderChoices(line, lineIndex) {
            const choices = this.parseChoice(line);
            if (choices.length === 0) return [];

            const buttons = [];
            const startY = config.padding + config.fontSize + (lineIndex * config.lineHeight);
            
            choices.forEach(choice => {
                const patternIndex = line.indexOf(choice.fullMatch);
                if (patternIndex === -1) return;

                const startX = config.padding + (patternIndex * config.fontSize * 0.6);
                let currentX = startX;

                choice.options.forEach(option => {
                    const optionWidth = option.length * config.fontSize * 0.6;
                    
                    buttons.push({
                        text: option,
                        x: currentX - 2,
                        y: startY - config.fontSize - 2,
                        width: optionWidth + 4,
                        height: config.fontSize + 4,
                        choiceName: choice.name,
                        hover: false
                    });

                    currentX += optionWidth + (config.fontSize * 0.6 * 4); // Account for " or "
                });
            });

            return buttons;
        }
    }

    // --- Button Management ---
    
    function addButton(button) {
        buttons.push(button);
    }

    function clearButtons() {
        buttons = [];
    }

    function findButtonAtPosition(x, y) {
        return buttons.find(button => 
            x >= button.x && x <= button.x + button.width &&
            y >= button.y && y <= button.y + button.height
        );
    }

    // --- Drawing Functions ---
    
    function resizeCanvas() {
        const dpr = window.devicePixelRatio || 1;
        const rect = canvas.getBoundingClientRect();
        
        canvas.width = rect.width * dpr;
        canvas.height = rect.height * dpr;
        ctx.scale(dpr, dpr);

        const usableHeight = rect.height - (config.padding * 2) - config.fontSize;
        maxLines = Math.floor(usableHeight / config.lineHeight) + 1;
    }

    function draw() {
        const rect = canvas.getBoundingClientRect();
        
        // Clear and draw background
        ctx.fillStyle = config.backgroundColor;
        ctx.fillRect(0, 0, rect.width, rect.height);

        // Draw status bar
        ctx.fillStyle = config.statusBarColor;
        ctx.fillRect(0, rect.height - config.lineHeight, rect.width, config.lineHeight);

        // Draw text
        ctx.font = `${config.fontSize}px ${config.fontFamily}`;
        ctx.fillStyle = config.textColor;
        ctx.textAlign = 'left';
        ctx.textBaseline = 'middle';

        const startY = config.padding + config.fontSize;
        lines.forEach((line, index) => {
            ctx.fillText(line, config.padding, startY + index * config.lineHeight);
        });

        // Draw choice boxes
        drawChoiceBoxes();

        // Draw cursor
        if (cursorVisible) {
            const cursorX = config.padding + ctx.measureText(lines[lines.length - 1]).width;
            const cursorY = startY + (lines.length - 1) * config.lineHeight;
            ctx.fillText(config.cursorChar, cursorX, cursorY);
        }
    }

    function drawChoiceBoxes() {
        // Clear old buttons and render new ones
        clearButtons();
        
        lines.forEach((line, lineIndex) => {
            const newButtons = ChoiceRenderer.renderChoices(line, lineIndex);
            newButtons.forEach(button => addButton(button));
        });

        // Draw boxes around choices
        buttons.forEach(button => {
            // Box
            ctx.strokeStyle = HORYZON_COLORS.GOLDEN_OCHRE;
            ctx.lineWidth = 2;
            ctx.strokeRect(button.x, button.y, button.width, button.height);

            // Hover effect
            if (button.hover) {
                ctx.fillStyle = HORYZON_COLORS.GOLDEN_OCHRE + '20';
                ctx.fillRect(button.x, button.y, button.width, button.height);
            }
        });
    }

    // --- Core Logic ---
    
    function addNewLine() {
        if (lines.length >= maxLines && maxLines > 0) {
            lines = [''];
        } else {
            lines.push('');
        }
    }

    function typeNextCharacter() {
        if (!currentTypingText) return;

        const char = currentTypingText[charIndex];
        if (char === '\n') {
            addNewLine();
        } else {
            lines[lines.length - 1] += char;
        }

        charIndex++;

        if (charIndex >= currentTypingText.length) {
            currentTypingText = null;
            charIndex = 0;
            addNewLine();
        }
    }

    function gameLoop(timestamp) {
        // Cursor blink
        if (timestamp - lastBlinkTime > config.cursorBlinkRate) {
            cursorVisible = !cursorVisible;
            lastBlinkTime = timestamp;
        }

        // Typing
        if (currentTypingText && timestamp - lastTypeTime > config.typingSpeed) {
            typeNextCharacter();
            lastTypeTime = timestamp;
        } else if (!currentTypingText && textQueue.length > 0) {
            currentTypingText = textQueue.shift();
            charIndex = 0;
        }

        draw();
        animationFrameId = requestAnimationFrame(gameLoop);
    }

    // --- Event Handling ---
    
    function handleMouseMove(event) {
        const rect = canvas.getBoundingClientRect();
        const mouseX = event.clientX - rect.left;
        const mouseY = event.clientY - rect.top;

        buttons.forEach(button => {
            button.hover = (mouseX >= button.x && mouseX <= button.x + button.width &&
                           mouseY >= button.y && mouseY <= button.y + button.height);
        });
    }

    function handleMouseClick(event) {
        const rect = canvas.getBoundingClientRect();
        const mouseX = event.clientX - rect.left;
        const mouseY = event.clientY - rect.top;

        const clickedButton = findButtonAtPosition(mouseX, mouseY);
        if (clickedButton) {
            const choiceData = {
                choice: {
                    [clickedButton.choiceName]: clickedButton.text
                }
            };

            if (config.onChoiceSelected) {
                config.onChoiceSelected(choiceData);
            }

            addNewLine();
            lines[lines.length - 1] = `Selected ${clickedButton.choiceName}: ${clickedButton.text}`;
            clearButtons();
        }
    }

    // --- Controller API ---
    
    const controller = {
        start: () => {
            if (animationFrameId) return;
            
            resizeCanvas();
            window.addEventListener('resize', resizeCanvas);
            canvas.addEventListener('mousemove', handleMouseMove);
            canvas.addEventListener('click', handleMouseClick);
            
            animationFrameId = requestAnimationFrame(gameLoop);
        },

        stop: () => {
            if (animationFrameId) {
                cancelAnimationFrame(animationFrameId);
                animationFrameId = null;
            }
            window.removeEventListener('resize', resizeCanvas);
            canvas.removeEventListener('mousemove', handleMouseMove);
            canvas.removeEventListener('click', handleMouseClick);
        },

        type: (text) => {
            if (typeof text === 'string' && text.length > 0) {
                textQueue.push(text);
            }
        },

        clear: () => {
            lines = [''];
            textQueue = [];
            currentTypingText = null;
            charIndex = 0;
            clearButtons();
        },

        getLines: () => [...lines],

        renderChoices: () => draw()
    };

    return controller;
}

// --- Main Entry Point ---

function main() {
    try {
        document.fonts.ready.then(() => {
            backBuzz('static-canvas');
            autoGlyf('glyf-area', 'The Intelligent Programming Program', 'Even AI needs a little AI');
            
            const terminalCanvas = document.getElementById('term-area');
            if (terminalCanvas) {
                const terminal = autoChat(terminalCanvas);
                terminal.start();
                terminal.type('¯\\_(ツ)_/¯');
                terminal.type('I am online');
            }
        });
    } catch (error) {
        console.error("Application initialization error:", error);
        document.body.innerHTML = '<div style="color: red; text-align: center; padding-top: 50px;"><h1>Error</h1><p>Check console for details.</p></div>';
    }
}

// Export functions
export {
    autoGlyf,
    createAsciiBox,
    wordWrap,
    backBuzz,
    autoChat,
    main,
    HORYZON_COLORS
};
